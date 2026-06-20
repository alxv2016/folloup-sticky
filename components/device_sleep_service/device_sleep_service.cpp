#include "device_sleep_service.h"

#include <algorithm>
#include <mutex>

#include "esp_log.h"
#include "esp_timer.h"

namespace device_sleep_service {
namespace {

constexpr const char* kTag = "DeviceSleep";
constexpr int64_t kMonitorIntervalUs = 1000 * 1000;

std::mutex s_mutex;
esp_timer_handle_t s_monitor_timer = nullptr;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
BlockerProvider s_blocker_provider = nullptr;
void* s_blocker_context = nullptr;
Settings s_settings = {};
Stage s_stage = Stage::kAwake;
TransitionReason s_last_transition_reason = TransitionReason::kNone;
BlockerReason s_blocker_reason = BlockerReason::kNone;
int64_t s_last_activity_us = 0;
int64_t s_inactivity_started_us = 0;
int64_t s_last_transition_us = 0;
bool s_initialized = false;
bool s_inactivity_armed = false;

uint32_t SecondsFromUs(int64_t duration_us)
{
    if (duration_us <= 0) {
        return 0;
    }
    return static_cast<uint32_t>(duration_us / 1000000LL);
}

uint32_t SecondsUntilTimeout(int64_t inactive_us, uint32_t timeout_seconds)
{
    if (timeout_seconds == 0) {
        return 0;
    }

    const int64_t timeout_us = static_cast<int64_t>(timeout_seconds) * 1000000LL;
    if (inactive_us >= timeout_us) {
        return 0;
    }
    return SecondsFromUs(timeout_us - inactive_us);
}

void EnsureActivityTimestampLocked(int64_t now_us)
{
    if (s_last_activity_us == 0) {
        s_last_activity_us = now_us;
    }
    if (s_last_transition_us == 0) {
        s_last_transition_us = now_us;
    }
}

Snapshot BuildSnapshotLocked(int64_t now_us)
{
    Snapshot snapshot = {};
    snapshot.settings = s_settings;
    snapshot.runtime.initialized = s_initialized;
    snapshot.runtime.stage = s_stage;
    snapshot.runtime.inactivity_armed = s_inactivity_armed;
    snapshot.runtime.last_transition_reason = s_last_transition_reason;
    snapshot.runtime.blocker_reason = s_blocker_reason;
    snapshot.runtime.blocked = s_blocker_reason != BlockerReason::kNone;

    const int64_t inactivity_anchor_us =
        s_inactivity_armed && s_inactivity_started_us > 0 ? s_inactivity_started_us : 0;
    if (inactivity_anchor_us > 0) {
        const int64_t inactive_us = std::max<int64_t>(0, now_us - inactivity_anchor_us);
        snapshot.runtime.inactive_seconds = SecondsFromUs(inactive_us);
        snapshot.runtime.seconds_until_display_sleep =
            SecondsUntilTimeout(inactive_us, s_settings.display_sleep_timeout_seconds);
        snapshot.runtime.seconds_until_light_sleep =
            SecondsUntilTimeout(inactive_us, s_settings.light_sleep_timeout_seconds);
    } else {
        snapshot.runtime.seconds_until_display_sleep =
            s_settings.display_sleep_timeout_seconds;
        snapshot.runtime.seconds_until_light_sleep = s_settings.light_sleep_timeout_seconds;
    }

    return snapshot;
}

void DispatchEvent(Action action, TransitionReason reason, const Snapshot& snapshot)
{
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_event_handler;
        context = s_event_context;
    }

    if (handler == nullptr) {
        return;
    }

    Event event = {};
    event.snapshot = snapshot;
    event.action = action;
    event.reason = reason;
    handler(event, context);
}

BlockerReason QueryBlocker()
{
    BlockerProvider provider = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        provider = s_blocker_provider;
        context = s_blocker_context;
    }

    if (provider == nullptr) {
        return BlockerReason::kNone;
    }
    return provider(context);
}

void UpdateBlockerLocked(BlockerReason blocker_reason, int64_t now_us)
{
    if (s_blocker_reason != blocker_reason) {
        ESP_LOGI(kTag, "Sleep blocker changed: %s -> %s",
                 BlockerReasonName(s_blocker_reason),
                 BlockerReasonName(blocker_reason));
        s_blocker_reason = blocker_reason;
    }

    if (s_blocker_reason != BlockerReason::kNone) {
        s_last_activity_us = now_us;
        if (s_inactivity_armed) {
            s_inactivity_started_us = now_us;
        }
    }
}

bool TransitionToLocked(Stage next_stage,
                        Action action,
                        TransitionReason reason,
                        int64_t now_us)
{
    if (s_stage == next_stage) {
        return false;
    }

    const Stage previous_stage = s_stage;
    s_stage = next_stage;
    s_last_transition_reason = reason;
    s_last_transition_us = now_us;

    if (action == Action::kWakeDisplay || action == Action::kWakeFromLightSleep) {
        s_last_activity_us = now_us;
        s_inactivity_armed = false;
        s_inactivity_started_us = 0;
    }

    ESP_LOGI(kTag,
             "Stage transition: %s -> %s action=%s reason=%s inactive=%us "
             "display_in=%us light_in=%us",
             StageName(previous_stage),
             StageName(s_stage),
             ActionName(action),
             TransitionReasonName(reason),
             static_cast<unsigned>(s_inactivity_started_us > 0
                                        ? SecondsFromUs(now_us - s_inactivity_started_us)
                                        : 0),
             static_cast<unsigned>(s_settings.display_sleep_timeout_seconds),
             static_cast<unsigned>(s_settings.light_sleep_timeout_seconds));
    return true;
}

void HandleMonitorTick()
{
    Action action = Action::kNone;
    Snapshot snapshot = {};
    const int64_t now_us = esp_timer_get_time();
    const BlockerReason blocker_reason = QueryBlocker();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized || !s_settings.enabled || !s_inactivity_armed ||
            s_inactivity_started_us == 0) {
            UpdateBlockerLocked(blocker_reason, now_us);
            return;
        }

        EnsureActivityTimestampLocked(now_us);
        UpdateBlockerLocked(blocker_reason, now_us);
        if (s_blocker_reason != BlockerReason::kNone) {
            return;
        }

        const int64_t inactive_us =
            std::max<int64_t>(0, now_us - s_inactivity_started_us);
        if (s_settings.light_sleep_timeout_seconds > 0 &&
            inactive_us >=
                static_cast<int64_t>(s_settings.light_sleep_timeout_seconds) * 1000000LL &&
            s_stage == Stage::kDisplaySleeping) {
            if (TransitionToLocked(Stage::kLightSleeping, Action::kEnterLightSleep,
                                   TransitionReason::kInactivity, now_us)) {
                action = Action::kEnterLightSleep;
            }
        } else if (s_settings.display_sleep_timeout_seconds > 0 &&
                   inactive_us >=
                       static_cast<int64_t>(s_settings.display_sleep_timeout_seconds) *
                           1000000LL &&
                   s_stage == Stage::kAwake) {
            if (TransitionToLocked(Stage::kDisplaySleeping, Action::kEnterDisplaySleep,
                                   TransitionReason::kInactivity, now_us)) {
                action = Action::kEnterDisplaySleep;
            }
        }

        if (action == Action::kNone) {
            return;
        }
        snapshot = BuildSnapshotLocked(now_us);
    }

    DispatchEvent(action, TransitionReason::kInactivity, snapshot);
}

void OnMonitorTimer(void*)
{
    HandleMonitorTick();
}

}  // namespace

esp_err_t Init()
{
    Settings applied_settings = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_initialized) {
            return ESP_OK;
        }

        if (s_settings.display_sleep_timeout_seconds > 0 &&
            s_settings.light_sleep_timeout_seconds > 0 &&
            s_settings.light_sleep_timeout_seconds <
                s_settings.display_sleep_timeout_seconds) {
            ESP_LOGW(kTag,
                     "Invalid sleep settings: light sleep timeout (%us) is shorter "
                     "than display sleep timeout (%us)",
                     static_cast<unsigned>(s_settings.light_sleep_timeout_seconds),
                     static_cast<unsigned>(s_settings.display_sleep_timeout_seconds));
            return ESP_ERR_INVALID_ARG;
        }

        if (s_monitor_timer == nullptr) {
            esp_timer_create_args_t timer_args = {};
            timer_args.callback = &OnMonitorTimer;
            timer_args.dispatch_method = ESP_TIMER_TASK;
            timer_args.name = "device_sleep";
            timer_args.skip_unhandled_events = true;

            const esp_err_t err = esp_timer_create(&timer_args, &s_monitor_timer);
            if (err != ESP_OK) {
                ESP_LOGW(kTag, "Monitor timer create failed: %s", esp_err_to_name(err));
                return err;
            }
        }

        const int64_t now_us = esp_timer_get_time();
        s_last_activity_us = now_us;
        s_inactivity_started_us = 0;
        s_last_transition_us = now_us;
        s_inactivity_armed = false;
        s_stage = Stage::kAwake;
        s_last_transition_reason = TransitionReason::kNone;
        s_blocker_reason = BlockerReason::kNone;

        const esp_err_t err = esp_timer_start_periodic(s_monitor_timer, kMonitorIntervalUs);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Monitor timer start failed: %s", esp_err_to_name(err));
            return err;
        }

        s_initialized = true;
        applied_settings = s_settings;
    }

    ESP_LOGI(kTag,
             "Initialized: enabled=%d display_timeout=%us light_timeout=%us "
             "motion_wake=%d interaction_wake=%d",
             applied_settings.enabled ? 1 : 0,
             static_cast<unsigned>(applied_settings.display_sleep_timeout_seconds),
             static_cast<unsigned>(applied_settings.light_sleep_timeout_seconds),
             applied_settings.motion_wake_enabled ? 1 : 0,
             applied_settings.interaction_wake_enabled ? 1 : 0);
    return ESP_OK;
}

esp_err_t ApplySettings(const Settings& settings)
{
    if (settings.display_sleep_timeout_seconds > 0 &&
        settings.light_sleep_timeout_seconds > 0 &&
        settings.light_sleep_timeout_seconds < settings.display_sleep_timeout_seconds) {
        ESP_LOGW(kTag,
                 "Rejected settings: light sleep timeout (%us) is shorter than "
                 "display sleep timeout (%us)",
                 static_cast<unsigned>(settings.light_sleep_timeout_seconds),
                 static_cast<unsigned>(settings.display_sleep_timeout_seconds));
        return ESP_ERR_INVALID_ARG;
    }

    Snapshot snapshot = {};
    Action action = Action::kNone;
    const int64_t now_us = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_settings = settings;
        EnsureActivityTimestampLocked(now_us);

        if (!s_settings.enabled && s_stage != Stage::kAwake) {
            action = s_stage == Stage::kLightSleeping ? Action::kWakeFromLightSleep
                                                      : Action::kWakeDisplay;
            TransitionToLocked(Stage::kAwake, action, TransitionReason::kSettingsChange,
                               now_us);
        }

        snapshot = BuildSnapshotLocked(now_us);
    }

    ESP_LOGI(kTag,
             "Settings applied: enabled=%d display_timeout=%us light_timeout=%us "
             "motion_wake=%d interaction_wake=%d",
             settings.enabled ? 1 : 0,
             static_cast<unsigned>(settings.display_sleep_timeout_seconds),
             static_cast<unsigned>(settings.light_sleep_timeout_seconds),
             settings.motion_wake_enabled ? 1 : 0,
             settings.interaction_wake_enabled ? 1 : 0);

    DispatchEvent(action, TransitionReason::kSettingsChange, snapshot);
    return ESP_OK;
}

Snapshot GetSnapshot()
{
    const int64_t now_us = esp_timer_get_time();
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildSnapshotLocked(now_us);
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

void SetBlockerProvider(BlockerProvider provider, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_blocker_provider = provider;
    s_blocker_context = context;
}

bool NotifyNoMotionStarted()
{
    const int64_t now_us = esp_timer_get_time();
    const BlockerReason blocker_reason = QueryBlocker();
    uint32_t display_sleep_timeout_seconds = 0;
    uint32_t light_sleep_timeout_seconds = 0;
    bool blocked = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        EnsureActivityTimestampLocked(now_us);
        if (!s_initialized || !s_settings.enabled || s_stage != Stage::kAwake ||
            s_inactivity_armed) {
            return false;
        }

        s_inactivity_armed = true;
        s_inactivity_started_us = now_us;
        UpdateBlockerLocked(blocker_reason, now_us);
        blocked = s_blocker_reason != BlockerReason::kNone;
        display_sleep_timeout_seconds = s_settings.display_sleep_timeout_seconds;
        light_sleep_timeout_seconds = s_settings.light_sleep_timeout_seconds;
    }

    ESP_LOGI(kTag,
             "No-motion armed inactivity countdown: display_sleep_in=%us "
             "light_sleep_in=%us blocked=%d blocker=%s",
             static_cast<unsigned>(display_sleep_timeout_seconds),
             static_cast<unsigned>(light_sleep_timeout_seconds),
             blocked ? 1 : 0,
             BlockerReasonName(blocker_reason));
    return true;
}

void NotifyUserActivity(ActivitySource source)
{
    Action action = Action::kNone;
    TransitionReason reason = TransitionReason::kInteraction;
    Snapshot snapshot = {};
    const int64_t now_us = esp_timer_get_time();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        EnsureActivityTimestampLocked(now_us);
        s_last_activity_us = now_us;
        s_inactivity_armed = false;
        s_inactivity_started_us = 0;

        if (source == ActivitySource::kMotion) {
            reason = TransitionReason::kMotion;
        }

        if (s_stage == Stage::kLightSleeping &&
            source != ActivitySource::kMotion &&
            s_settings.interaction_wake_enabled) {
            if (TransitionToLocked(Stage::kAwake, Action::kWakeFromLightSleep, reason,
                                   now_us)) {
                action = Action::kWakeFromLightSleep;
            }
        } else if (s_stage == Stage::kDisplaySleeping &&
                   ((source == ActivitySource::kMotion && s_settings.motion_wake_enabled) ||
                    (source != ActivitySource::kMotion &&
                     s_settings.interaction_wake_enabled))) {
            if (TransitionToLocked(Stage::kAwake, Action::kWakeDisplay, reason, now_us)) {
                action = Action::kWakeDisplay;
            }
        }

        if (action == Action::kNone) {
            return;
        }
        snapshot = BuildSnapshotLocked(now_us);
    }

    ESP_LOGI(kTag, "Wake requested: source=%s action=%s",
             ActivitySourceName(source), ActionName(action));
    DispatchEvent(action, reason, snapshot);
}

bool NotifyLightSleepWake(TransitionReason reason)
{
    const int64_t now_us = esp_timer_get_time();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        EnsureActivityTimestampLocked(now_us);
        if (s_stage != Stage::kLightSleeping) {
            ESP_LOGW(kTag, "Light-sleep wake ignored from stage=%s", StageName(s_stage));
            return false;
        }

        TransitionToLocked(Stage::kAwake, Action::kWakeFromLightSleep, reason, now_us);
    }

    ESP_LOGI(kTag, "Light-sleep wake committed: reason=%s",
             TransitionReasonName(reason));
    return true;
}

void NotifyMotionDetected()
{
    NotifyUserActivity(ActivitySource::kMotion);
}

const char* StageName(Stage stage)
{
    switch (stage) {
        case Stage::kAwake:
            return "awake";
        case Stage::kDisplaySleeping:
            return "display_sleeping";
        case Stage::kLightSleeping:
            return "light_sleeping";
        default:
            return "unknown";
    }
}

const char* ActionName(Action action)
{
    switch (action) {
        case Action::kNone:
            return "none";
        case Action::kEnterDisplaySleep:
            return "enter_display_sleep";
        case Action::kWakeDisplay:
            return "wake_display";
        case Action::kEnterLightSleep:
            return "enter_light_sleep";
        case Action::kWakeFromLightSleep:
            return "wake_from_light_sleep";
        default:
            return "unknown";
    }
}

const char* TransitionReasonName(TransitionReason reason)
{
    switch (reason) {
        case TransitionReason::kNone:
            return "none";
        case TransitionReason::kInactivity:
            return "inactivity";
        case TransitionReason::kMotion:
            return "motion";
        case TransitionReason::kInteraction:
            return "interaction";
        case TransitionReason::kSettingsChange:
            return "settings_change";
        default:
            return "unknown";
    }
}

const char* ActivitySourceName(ActivitySource source)
{
    switch (source) {
        case ActivitySource::kUnknown:
            return "unknown";
        case ActivitySource::kInteraction:
            return "interaction";
        case ActivitySource::kMotion:
            return "motion";
        default:
            return "unknown";
    }
}

const char* BlockerReasonName(BlockerReason reason)
{
    switch (reason) {
        case BlockerReason::kNone:
            return "none";
        case BlockerReason::kRecordingActive:
            return "recording_active";
        case BlockerReason::kRecordingSaving:
            return "recording_saving";
        case BlockerReason::kShutdownPending:
            return "shutdown_pending";
        case BlockerReason::kDisplayRefresh:
            return "display_refresh";
        case BlockerReason::kStorageWrite:
            return "storage_write";
        case BlockerReason::kWifiAccessPoint:
            return "wifi_access_point";
        case BlockerReason::kTimeSync:
            return "time_sync";
        default:
            return "unknown";
    }
}

}  // namespace device_sleep_service
