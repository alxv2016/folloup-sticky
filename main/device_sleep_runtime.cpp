#include "device_sleep_runtime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>

#include "device_sleep_service.h"
#include "display_service.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "recording_service.h"
#include "status_bar_runtime.h"
#include "sticky_board_config.h"
#include "storage_service.h"
#include "timezone_service.h"
#include "touch_service.h"
#include "wifi_service.h"

namespace device_sleep_runtime {
namespace {

constexpr const char* kTag = "DeviceSleepRuntime";
constexpr TickType_t kMotionPollInterval = pdMS_TO_TICKS(200);
constexpr int64_t kStillWindowUs = 2 * 1000 * 1000;
constexpr float kMotionStartSumDeltaMg = 60.0f;
constexpr float kMotionStartMaxAxisDeltaMg = 25.0f;
constexpr float kStillSumDeltaMg = 20.0f;
constexpr float kStillMaxAxisDeltaMg = 8.0f;
constexpr uint32_t kMotionTaskStackWords = 4096;
constexpr uint32_t kAutoSleepTaskStackWords = 4096;
constexpr size_t kAutoSleepEventQueueDepth = 8;
constexpr TickType_t kPowerButtonReleasePollDelay = pdMS_TO_TICKS(20);
constexpr uint32_t kPowerButtonReleaseStableSamples = 10;
constexpr uint32_t kPowerButtonReleaseMaxSamples = 250;
constexpr uint64_t kPowerButtonWakeMask = 1ULL << STICKY_POWER_BUTTON_PIN;
// Upper bound on how long a wake gesture may keep POWER_OK suppressed. A press ->
// release -> double-click-window sequence resolves well inside this; the deadline
// only exists so a gesture whose terminal event never arrives (a triple tap emits
// neither SINGLE_CLICK nor DOUBLE_CLICK) cannot strand the suppression forever.
constexpr int64_t kWakeGestureTimeoutUs = 2 * 1000 * 1000;

enum class MotionState {
    kUnknown,
    kMoving,
    kStill,
};

struct AccelSampleMg {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    uint32_t timestamp_ms = 0;
};

TaskHandle_t s_auto_sleep_task = nullptr;
QueueHandle_t s_auto_sleep_event_queue = nullptr;
TaskHandle_t s_motion_task = nullptr;
std::mutex s_motion_mutex;
std::mutex s_shutdown_provider_mutex;
ShutdownPendingProvider s_shutdown_pending_provider = nullptr;
void* s_shutdown_pending_context = nullptr;
std::atomic<bool> s_wake_gesture_active = false;
std::atomic<bool> s_wake_gesture_long_press = false;
std::atomic<int64_t> s_wake_gesture_deadline_us = 0;
bool s_have_last_motion_sample = false;
AccelSampleMg s_last_motion_sample = {};
int64_t s_quiet_started_us = 0;
MotionState s_motion_state = MotionState::kUnknown;
unsigned s_consecutive_read_errors = 0;

const char* ButtonEventName(button_service::ButtonEvent event)
{
    switch (event) {
        case button_service::ButtonEvent::kPressDown:
            return "PRESS_DOWN";
        case button_service::ButtonEvent::kPressUp:
            return "PRESS_UP";
        case button_service::ButtonEvent::kPressRepeat:
            return "PRESS_REPEAT";
        case button_service::ButtonEvent::kSingleClick:
            return "SINGLE_CLICK";
        case button_service::ButtonEvent::kDoubleClick:
            return "DOUBLE_CLICK";
        case button_service::ButtonEvent::kLongPressStart:
            return "LONG_PRESS_START";
        case button_service::ButtonEvent::kLongPressUp:
            return "LONG_PRESS_UP";
        default:
            return "UNKNOWN";
    }
}

const char* WakeupCauseName(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
        case ESP_SLEEP_WAKEUP_GPIO:
            return "gpio";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "timer";
        case ESP_SLEEP_WAKEUP_EXT1:
            return "ext1";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "undefined";
        default:
            return "other";
    }
}

bool IsShutdownPending()
{
    ShutdownPendingProvider provider = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_shutdown_provider_mutex);
        provider = s_shutdown_pending_provider;
        context = s_shutdown_pending_context;
    }

    return provider != nullptr && provider(context);
}

device_sleep_service::BlockerReason GetAutoSleepBlocker(void*)
{
    if (IsShutdownPending()) {
        return device_sleep_service::BlockerReason::kShutdownPending;
    }

    if (recording_service::IsInitialized()) {
        const recording_service::UiState recording_state = recording_service::GetUiState();
        if (recording_state.saving || recording_state.exporting) {
            return device_sleep_service::BlockerReason::kRecordingSaving;
        }
        if (recording_state.armed || recording_state.recording) {
            return device_sleep_service::BlockerReason::kRecordingActive;
        }
    }

    if (storage_service::IsWriteBusy()) {
        return device_sleep_service::BlockerReason::kStorageWrite;
    }

    if (wifi_service::IsAccessPointMode()) {
        return device_sleep_service::BlockerReason::kWifiAccessPoint;
    }

    if (timezone_service::IsSyncInProgress()) {
        return device_sleep_service::BlockerReason::kTimeSync;
    }

    if (display_service::IsRefreshInProgress()) {
        return device_sleep_service::BlockerReason::kDisplayRefresh;
    }

    return device_sleep_service::BlockerReason::kNone;
}

void HandleAutoSleepEvent(const device_sleep_service::Event& event, void*)
{
    if (s_auto_sleep_event_queue == nullptr) {
        ESP_LOGW(kTag, "Auto-sleep event dropped; queue unavailable");
        return;
    }

    if (xQueueSend(s_auto_sleep_event_queue, &event, 0) != pdPASS) {
        ESP_LOGW(kTag, "Auto-sleep event dropped; queue full");
    }
}

void LogLightSleepPins(const char* phase)
{
    ESP_LOGI(kTag,
             "Light-sleep %s pins: POWER_OK(GPIO%d)=%d PWR_HOLD(GPIO%d)=%d "
             "PWR_LOCK(GPIO%d)=%d",
             phase,
             STICKY_POWER_BUTTON_PIN,
             gpio_get_level(STICKY_POWER_BUTTON_PIN),
             STICKY_POWER_HOLD_PIN,
             gpio_get_level(STICKY_POWER_HOLD_PIN),
             STICKY_POWER_LOCK_PIN,
             gpio_get_level(STICKY_POWER_LOCK_PIN));
}

esp_err_t ConfigurePowerButtonWakeInput(const char* context)
{
    ESP_LOGI(kTag, "Light-sleep wake input config begin: %s", context);
    gpio_config_t power_button_config = {};
    power_button_config.pin_bit_mask = 1ULL << STICKY_POWER_BUTTON_PIN;
    power_button_config.mode = GPIO_MODE_INPUT;
    power_button_config.pull_up_en = GPIO_PULLUP_ENABLE;
    power_button_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_button_config.intr_type = GPIO_INTR_DISABLE;

    const esp_err_t err = gpio_config(&power_button_config);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Configure POWER_OK %s input failed: %s",
                 context, esp_err_to_name(err));
    } else {
        LogLightSleepPins("after POWER_OK input config");
    }
    return err;
}

esp_err_t ConfigureSleepOutputHigh(gpio_num_t pin, const char* label)
{
    ESP_LOGI(kTag, "Light-sleep latch config begin: %s GPIO%d output high",
             label, pin);

    esp_err_t err = gpio_set_level(pin, 1);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Set %s GPIO%d high before light sleep failed: %s",
                 label, pin, esp_err_to_name(err));
        return err;
    }

    err = gpio_sleep_set_direction(pin, GPIO_MODE_OUTPUT);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Set %s GPIO%d sleep output mode failed: %s",
                 label, pin, esp_err_to_name(err));
        return err;
    }

    err = gpio_sleep_set_pull_mode(pin, GPIO_FLOATING);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Set %s GPIO%d sleep pull mode failed: %s",
                 label, pin, esp_err_to_name(err));
        return err;
    }

    err = gpio_sleep_sel_en(pin);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Enable %s GPIO%d sleep config failed: %s",
                 label, pin, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "Light-sleep latch config done: %s GPIO%d level=%d",
             label, pin, gpio_get_level(pin));
    return ESP_OK;
}

esp_err_t PreservePowerLatchDuringLightSleep()
{
    LogLightSleepPins("before latch sleep config");
    ESP_RETURN_ON_ERROR(ConfigureSleepOutputHigh(STICKY_POWER_HOLD_PIN, "PWR_HOLD"),
                        kTag,
                        "preserve PWR_HOLD during light sleep failed");
    ESP_RETURN_ON_ERROR(ConfigureSleepOutputHigh(STICKY_POWER_LOCK_PIN, "PWR_LOCK"),
                        kTag,
                        "preserve PWR_LOCK during light sleep failed");

    ESP_LOGI(kTag, "Power latch sleep hold armed: PWR_HOLD(GPIO%d)=1 PWR_LOCK(GPIO%d)=1",
             STICKY_POWER_HOLD_PIN,
             STICKY_POWER_LOCK_PIN);
    LogLightSleepPins("after latch sleep config");
    return ESP_OK;
}

esp_err_t WaitForPowerButtonReleased()
{
    ESP_LOGI(kTag, "Light-sleep POWER_OK release wait begin");
    uint32_t stable_high_samples = 0;
    for (uint32_t sample = 0; sample < kPowerButtonReleaseMaxSamples; ++sample) {
        const int level = gpio_get_level(STICKY_POWER_BUTTON_PIN);
        if (level == 1) {
            ++stable_high_samples;
            if (stable_high_samples >= kPowerButtonReleaseStableSamples) {
                ESP_LOGI(kTag,
                         "Light-sleep POWER_OK released: stable_samples=%lu "
                         "elapsed_ms=%lu",
                         static_cast<unsigned long>(stable_high_samples),
                         static_cast<unsigned long>(
                             (sample + 1) * kPowerButtonReleasePollDelay *
                             portTICK_PERIOD_MS));
                LogLightSleepPins("after POWER_OK release wait");
                return ESP_OK;
            }
        } else {
            stable_high_samples = 0;
        }
        vTaskDelay(kPowerButtonReleasePollDelay);
    }

    ESP_LOGW(kTag, "POWER_OK stayed active; light sleep not armed");
    return ESP_ERR_TIMEOUT;
}

esp_err_t RestoreAfterLightSleep()
{
    ESP_LOGI(kTag, "Light-sleep restore begin");
    LogLightSleepPins("before restore");
    esp_err_t err = display_service::RecoverAfterLightSleep();
    if (touch_service::IsInitialized()) {
        const esp_err_t touch_err = touch_service::RecoverAfterLightSleep();
        if (touch_err != ESP_OK) {
            ESP_LOGW(kTag, "Touch recovery after light sleep failed: %s",
                     esp_err_to_name(touch_err));
        }
    }
    // The SD card shares the SPI bus the display just re-initialized; remount it
    // so the first post-wake read doesn't hit a stale card (sdmmc 0x107 timeout).
    const esp_err_t storage_err = storage_service::RecoverAfterLightSleep();
    if (storage_err != ESP_OK && storage_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Storage recovery after light sleep failed: %s",
                 esp_err_to_name(storage_err));
    }
    if (wifi_service::IsBusy() || wifi_service::IsConnected() || wifi_service::HasSavedCredentials() ||
        wifi_service::IsAccessPointMode()) {
        wifi_service::RecoverAfterLightSleep();
    }
    LogLightSleepPins("after restore");
    ESP_LOGI(kTag, "Light-sleep restore done: display_err=%s",
             esp_err_to_name(err));
    return err;
}

esp_err_t AbortLightSleepEntry(esp_err_t err, const char* reason)
{
    ESP_LOGW(kTag, "Aborting light sleep entry: %s: %s",
             reason, esp_err_to_name(err));
    esp_sleep_disable_ext1_wakeup_io(kPowerButtonWakeMask);
    status_bar_runtime::SetSleepIndicatorVisible(false);
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK) {
        ESP_LOGW(kTag, "Status bar reset after light-sleep abort failed: %s",
                 esp_err_to_name(status_bar_err));
    }

    const bool wake_committed =
        device_sleep_service::NotifyLightSleepWake(device_sleep_service::TransitionReason::kNone);
    if (!wake_committed) {
        ESP_LOGW(kTag, "Light-sleep abort did not transition service state");
    }

    const esp_err_t restore_err = RestoreAfterLightSleep();
    if (restore_err != ESP_OK) {
        ESP_LOGW(kTag, "Display restore after light-sleep abort failed: %s",
                 esp_err_to_name(restore_err));
    }
    return err;
}

esp_err_t EnterLightSleep()
{
    ESP_LOGI(kTag, "Light-sleep entry begin");
    LogLightSleepPins("entry");

    esp_err_t err = PreservePowerLatchDuringLightSleep();
    if (err != ESP_OK) {
        return AbortLightSleepEntry(err, "preserve power latch sleep state failed");
    }

    err = ConfigurePowerButtonWakeInput("light-sleep wake");
    if (err != ESP_OK) {
        return AbortLightSleepEntry(err, "configure POWER_OK wake input failed");
    }

    err = WaitForPowerButtonReleased();
    if (err != ESP_OK) {
        return AbortLightSleepEntry(err, "POWER_OK release wait failed");
    }

    err = esp_sleep_enable_ext1_wakeup_io(kPowerButtonWakeMask,
                                          ESP_EXT1_WAKEUP_ANY_LOW);
    if (err != ESP_OK) {
        return AbortLightSleepEntry(err, "enable POWER_OK EXT1 wake failed");
    }
    ESP_LOGI(kTag,
             "Light-sleep EXT1 wake armed: mask=0x%llX mode=ANY_LOW",
             static_cast<unsigned long long>(kPowerButtonWakeMask));

    ESP_LOGI(kTag, "Light-sleep display preparation begin");
    status_bar_runtime::SetSleepIndicatorVisible(true);
    err = status_bar_runtime::UpdateDisplayStateAndRefreshNow(display_service::RefreshMode::kPartial);
    if (err != ESP_OK) {
        status_bar_runtime::SetSleepIndicatorVisible(false);
        return AbortLightSleepEntry(err, "render status bar light-sleep indicator failed");
    }
    err = display_service::EnterLightSleep();
    if (err != ESP_OK) {
        return AbortLightSleepEntry(err, "display light sleep transition failed");
    }
    ESP_LOGI(kTag, "Light-sleep display preparation done");
    LogLightSleepPins("after display preparation");

    ESP_LOGI(kTag,
             "Light-sleep start now. Expect 'returned from esp_light_sleep_start' "
             "next; if logs show a boot banner instead, the board reset or lost power.");
    vTaskDelay(pdMS_TO_TICKS(50));
    // Freeze the button state machine across the sleep. esp_timer's clock is snapped
    // forward by the sleep duration on wake, and iot_button's 5ms tick timer does not
    // skip unhandled events -- left running it would replay one tick per missed 5ms in
    // a single burst, racing the still-held wake press past the long-press threshold
    // and destroying single/double-click classification for the whole gesture.
    (void)button_service::Suspend();
    const esp_err_t sleep_err = esp_light_sleep_start();

    // Restore the POWER_OK pad and the button poller before anything slow (logging is
    // ~10ms per line at 115200 baud). Until rtc_gpio_deinit runs the pad is still
    // routed to the RTC mux by ext1_wakeup_prepare, so the digital input reads 0 --
    // which is the active level -- and any polling would see a phantom press.
    const esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    const uint64_t ext1_status = esp_sleep_get_ext1_wakeup_status();
    esp_sleep_disable_ext1_wakeup_io(kPowerButtonWakeMask);

    const esp_err_t rtc_deinit_err = rtc_gpio_deinit(STICKY_POWER_BUTTON_PIN);
    const esp_err_t reconfig_err = ConfigurePowerButtonWakeInput("post-light-sleep");

    const bool power_button_held = gpio_get_level(STICKY_POWER_BUTTON_PIN) == 0;
    const bool power_button_wake =
        (wakeup_cause == ESP_SLEEP_WAKEUP_EXT1 &&
         (ext1_status & kPowerButtonWakeMask) != 0) ||
        power_button_held;

    // Commit the stage transition before buttons go live, so a wake press cannot race
    // in while the service still reports kLightSleeping and queue a second
    // kWakeFromLightSleep action (which would run the whole restore twice).
    const bool wake_committed = device_sleep_service::NotifyLightSleepWake(
        device_sleep_service::TransitionReason::kInteraction);
    // Arm on "still held", not on "was the wake source": a tap short enough to end
    // before polling resumes produces no events at all, and arming for it would
    // swallow the user's next deliberate press instead.
    if (power_button_held) {
        ArmPowerButtonWakeGesture("light-sleep wake");
    }
    (void)button_service::Resume();

    ESP_LOGI(kTag, "Light-sleep returned from esp_light_sleep_start: err=%s",
             esp_err_to_name(sleep_err));
    if (rtc_deinit_err != ESP_OK) {
        ESP_LOGW(kTag, "POWER_OK RTC GPIO deinit after light sleep failed: %s",
                 esp_err_to_name(rtc_deinit_err));
    }
    if (reconfig_err != ESP_OK) {
        ESP_LOGW(kTag, "POWER_OK digital input restore after light sleep failed: %s",
                 esp_err_to_name(reconfig_err));
    }
    ESP_LOGI(kTag, "Exited light sleep: cause=%s ext1_status=0x%llX power_button=%d err=%s",
             WakeupCauseName(wakeup_cause),
             static_cast<unsigned long long>(ext1_status),
             power_button_wake ? 1 : 0,
             esp_err_to_name(sleep_err));
    LogLightSleepPins("after wake source cleanup");
    if (!wake_committed) {
        ESP_LOGW(kTag, "Light-sleep wake did not transition service state");
    }

    status_bar_runtime::SetSleepIndicatorVisible(false);
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK) {
        ESP_LOGW(kTag, "Status bar clear after light sleep failed: %s",
                 esp_err_to_name(status_bar_err));
    }

    const esp_err_t restore_err = RestoreAfterLightSleep();
    ESP_LOGI(kTag, "Light-sleep exit complete: sleep_err=%s restore_err=%s",
             esp_err_to_name(sleep_err), esp_err_to_name(restore_err));
    return sleep_err == ESP_OK ? restore_err : sleep_err;
}

void ProcessAutoSleepEvent(const device_sleep_service::Event& event)
{
    ESP_LOGI(kTag, "Auto-sleep event: action=%s reason=%s stage=%s inactive=%us",
             device_sleep_service::ActionName(event.action),
             device_sleep_service::TransitionReasonName(event.reason),
             device_sleep_service::StageName(event.snapshot.runtime.stage),
             static_cast<unsigned>(event.snapshot.runtime.inactive_seconds));

    esp_err_t err = ESP_OK;
    switch (event.action) {
        case device_sleep_service::Action::kEnterDisplaySleep:
            status_bar_runtime::SetSleepIndicatorVisible(true);
            err = status_bar_runtime::UpdateDisplayStateAndRefreshNow(
                display_service::RefreshMode::kPartial);
            if (err != ESP_OK) {
                status_bar_runtime::SetSleepIndicatorVisible(false);
                break;
            }
            err = display_service::EnterDisplaySleep();
            break;
        case device_sleep_service::Action::kWakeDisplay:
            status_bar_runtime::SetSleepIndicatorVisible(false);
            (void)status_bar_runtime::UpdateDisplayState();
            err = display_service::WakeDisplay();
            break;
        case device_sleep_service::Action::kEnterLightSleep:
            err = EnterLightSleep();
            break;
        case device_sleep_service::Action::kWakeFromLightSleep:
            status_bar_runtime::SetSleepIndicatorVisible(false);
            (void)status_bar_runtime::UpdateDisplayState();
            err = RestoreAfterLightSleep();
            break;
        case device_sleep_service::Action::kNone:
        default:
            return;
    }

    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Auto-sleep action failed: %s", esp_err_to_name(err));
    }
}

void AutoSleepTask(void*)
{
    while (true) {
        device_sleep_service::Event event = {};
        if (xQueueReceive(s_auto_sleep_event_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        ProcessAutoSleepEvent(event);
    }
}

esp_err_t StartAutoSleepTask()
{
    if (s_auto_sleep_task != nullptr) {
        return ESP_OK;
    }

    if (s_auto_sleep_event_queue == nullptr) {
        s_auto_sleep_event_queue =
            xQueueCreate(kAutoSleepEventQueueDepth, sizeof(device_sleep_service::Event));
        if (s_auto_sleep_event_queue == nullptr) {
            ESP_LOGW(kTag, "Failed to create auto-sleep event queue");
            return ESP_ERR_NO_MEM;
        }
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        AutoSleepTask,
        "app_sleep",
        kAutoSleepTaskStackWords,
        nullptr,
        followup_task_config::kPriorityAppSleep,
        &s_auto_sleep_task,
        followup_task_config::kAppCore);
    if (created != pdPASS) {
        s_auto_sleep_task = nullptr;
        ESP_LOGW(kTag, "Failed to create auto-sleep task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

AccelSampleMg ToAccelSampleMg(const imu_service::ImuSample& sample)
{
    return {
        .x = sample.accel_x_g * 1000.0f,
        .y = sample.accel_y_g * 1000.0f,
        .z = sample.accel_z_g * 1000.0f,
        .timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000),
    };
}

void ResetMotionClassifier()
{
    std::lock_guard<std::mutex> lock(s_motion_mutex);
    s_have_last_motion_sample = false;
    s_last_motion_sample = {};
    s_quiet_started_us = 0;
    s_motion_state = MotionState::kUnknown;
}

void ClassifyMotionSample(const AccelSampleMg& sample)
{
    if (sample.timestamp_ms == 0) {
        return;
    }

    bool should_notify_motion = false;
    bool should_notify_no_motion = false;
    long long quiet_for_ms = 0;
    float log_sum_delta = 0.0f;
    float log_max_axis_delta = 0.0f;
    const int64_t now_us = esp_timer_get_time();
    {
        std::lock_guard<std::mutex> lock(s_motion_mutex);
        if (!s_have_last_motion_sample) {
            s_last_motion_sample = sample;
            s_have_last_motion_sample = true;
            s_quiet_started_us = now_us;
            return;
        }

        const float dx = std::fabs(sample.x - s_last_motion_sample.x);
        const float dy = std::fabs(sample.y - s_last_motion_sample.y);
        const float dz = std::fabs(sample.z - s_last_motion_sample.z);
        const float sum_delta = dx + dy + dz;
        const float max_axis_delta = std::max(dx, std::max(dy, dz));
        const bool motion_detected =
            sum_delta >= kMotionStartSumDeltaMg ||
            max_axis_delta >= kMotionStartMaxAxisDeltaMg;
        const bool still_detected =
            sum_delta <= kStillSumDeltaMg && max_axis_delta <= kStillMaxAxisDeltaMg;

        s_last_motion_sample = sample;
        log_sum_delta = sum_delta;
        log_max_axis_delta = max_axis_delta;

        if (motion_detected) {
            s_quiet_started_us = 0;
            if (s_motion_state != MotionState::kMoving) {
                s_motion_state = MotionState::kMoving;
                should_notify_motion = true;
            }
        } else if (!still_detected) {
            s_quiet_started_us = 0;
            if (s_motion_state == MotionState::kStill) {
                s_motion_state = MotionState::kUnknown;
            }
        } else {
            if (s_quiet_started_us == 0) {
                s_quiet_started_us = now_us;
            }
            if (s_motion_state != MotionState::kStill &&
                now_us - s_quiet_started_us >= kStillWindowUs) {
                s_motion_state = MotionState::kStill;
                quiet_for_ms =
                    static_cast<long long>((now_us - s_quiet_started_us) / 1000);
                should_notify_no_motion = true;
            }
        }
    }

    if (should_notify_motion) {
        ESP_LOGI(kTag, "Motion detected: sum_delta=%.1fmg max_axis=%.1fmg",
                 static_cast<double>(log_sum_delta),
                 static_cast<double>(log_max_axis_delta));
        device_sleep_service::NotifyMotionDetected();
        return;
    }

    if (should_notify_no_motion && device_sleep_service::NotifyNoMotionStarted()) {
        ESP_LOGI(kTag, "No-motion detected: quiet_for=%lldms sum_delta=%.1fmg max_axis=%.1fmg",
                 quiet_for_ms,
                 static_cast<double>(log_sum_delta),
                 static_cast<double>(log_max_axis_delta));
    }
}

void MotionPollingTask(void*)
{
    TickType_t last_wake_tick = xTaskGetTickCount();
    while (true) {
        vTaskDelayUntil(&last_wake_tick, kMotionPollInterval);

        if (!imu_service::IsInitialized()) {
            continue;
        }

        imu_service::ImuSample sample = {};
        const esp_err_t err = imu_service::ReadSample(&sample);
        if (err != ESP_OK) {
            ++s_consecutive_read_errors;
            if (s_consecutive_read_errors == 1 || s_consecutive_read_errors % 50 == 0) {
                ESP_LOGW(kTag, "IMU motion sample failed: %s consecutive_errors=%u",
                         esp_err_to_name(err),
                         s_consecutive_read_errors);
            }
            continue;
        }
        if (s_consecutive_read_errors != 0) {
            ESP_LOGI(kTag, "IMU motion sampling recovered after %u errors",
                     s_consecutive_read_errors);
            s_consecutive_read_errors = 0;
        }

        ClassifyMotionSample(ToAccelSampleMg(sample));
    }
}

}  // namespace

void SetShutdownPendingProvider(ShutdownPendingProvider provider, void* context)
{
    std::lock_guard<std::mutex> lock(s_shutdown_provider_mutex);
    s_shutdown_pending_provider = provider;
    s_shutdown_pending_context = context;
}

esp_err_t StartAutoSleep(const AutoSleepSettings& settings)
{
    ESP_RETURN_ON_ERROR(StartAutoSleepTask(), kTag, "auto-sleep task init failed");

    device_sleep_service::SetEventHandler(HandleAutoSleepEvent, nullptr);
    device_sleep_service::SetBlockerProvider(GetAutoSleepBlocker, nullptr);

    device_sleep_service::Settings service_settings = {};
    service_settings.enabled = settings.enabled;
    service_settings.display_sleep_timeout_seconds =
        settings.display_sleep_timeout_seconds;
    service_settings.light_sleep_timeout_seconds = settings.light_sleep_timeout_seconds;
    service_settings.motion_wake_enabled = settings.motion_wake_enabled;
    service_settings.interaction_wake_enabled = settings.interaction_wake_enabled;

    ESP_LOGI(kTag,
             "Auto-sleep settings resolved: enabled=%d display_timeout=%us "
             "light_timeout=%us motion_wake=%d interaction_wake=%d",
             service_settings.enabled ? 1 : 0,
             static_cast<unsigned>(service_settings.display_sleep_timeout_seconds),
             static_cast<unsigned>(service_settings.light_sleep_timeout_seconds),
             service_settings.motion_wake_enabled ? 1 : 0,
             service_settings.interaction_wake_enabled ? 1 : 0);

    ESP_RETURN_ON_ERROR(device_sleep_service::ApplySettings(service_settings),
                        kTag,
                        "auto-sleep settings failed");
    ESP_RETURN_ON_ERROR(device_sleep_service::Init(), kTag, "auto-sleep service init failed");
    return ESP_OK;
}

esp_err_t StartMotionPolling()
{
    if (s_motion_task != nullptr) {
        return ESP_OK;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        MotionPollingTask,
        "sleep_motion",
        kMotionTaskStackWords,
        nullptr,
        followup_task_config::kPrioritySleepMotion,
        &s_motion_task,
        followup_task_config::kAppCore);
    if (created != pdPASS) {
        s_motion_task = nullptr;
        ESP_LOGW(kTag, "Failed to create motion polling task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag,
             "Motion polling started: interval=%ums motion_sum=%.1fmg motion_axis=%.1fmg "
             "still_sum=%.1fmg still_axis=%.1fmg still_window=%lldms",
             static_cast<unsigned>(pdTICKS_TO_MS(kMotionPollInterval)),
             static_cast<double>(kMotionStartSumDeltaMg),
             static_cast<double>(kMotionStartMaxAxisDeltaMg),
             static_cast<double>(kStillSumDeltaMg),
             static_cast<double>(kStillMaxAxisDeltaMg),
             static_cast<long long>(kStillWindowUs / 1000));
    return ESP_OK;
}

void NotifyUserActivity()
{
    ResetMotionClassifier();
    device_sleep_service::NotifyUserActivity(device_sleep_service::ActivitySource::kInteraction);
}

void ArmPowerButtonWakeGesture(const char* reason)
{
    s_wake_gesture_long_press.store(false, std::memory_order_relaxed);
    s_wake_gesture_deadline_us.store(esp_timer_get_time() + kWakeGestureTimeoutUs,
                                     std::memory_order_relaxed);
    s_wake_gesture_active.store(true, std::memory_order_relaxed);
    ESP_LOGI(kTag, "POWER_OK wake gesture armed: %s", reason);
}

bool ConsumeWakeOnlyPowerButtonEvent(const button_service::ButtonEventInfo& event)
{
    if (event.button != button_service::ButtonId::kPowerOk ||
        !s_wake_gesture_active.load(std::memory_order_relaxed)) {
        return false;
    }

    if (esp_timer_get_time() >
        s_wake_gesture_deadline_us.load(std::memory_order_relaxed)) {
        ESP_LOGW(kTag, "POWER_OK wake gesture timed out; releasing suppression");
        s_wake_gesture_active.store(false, std::memory_order_relaxed);
        return false;
    }

    // iot_button orders a gesture's events PRESS_UP -> SINGLE_CLICK / DOUBLE_CLICK, and
    // LONG_PRESS_UP -> PRESS_UP. Suppression must therefore survive PRESS_UP, otherwise
    // the wake tap's trailing SINGLE_CLICK leaks into the app.
    switch (event.event) {
        case button_service::ButtonEvent::kDoubleClick:
            // The one event that is not swallowed: a deliberate double click is the
            // lock-screen toggle, so waking and unlocking stays a single gesture.
            s_wake_gesture_active.store(false, std::memory_order_relaxed);
            ESP_LOGI(kTag, "Wake gesture resolved as DOUBLE_CLICK; forwarding to app");
            return false;
        case button_service::ButtonEvent::kSingleClick:
            s_wake_gesture_active.store(false, std::memory_order_relaxed);
            break;
        case button_service::ButtonEvent::kLongPressStart:
            s_wake_gesture_long_press.store(true, std::memory_order_relaxed);
            break;
        case button_service::ButtonEvent::kPressUp:
            // A long press produces no click event, so its trailing PRESS_UP is the
            // gesture's terminal event. A tap's PRESS_UP is not: a click still follows.
            if (s_wake_gesture_long_press.load(std::memory_order_relaxed)) {
                s_wake_gesture_active.store(false, std::memory_order_relaxed);
            }
            break;
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kPressRepeat:
        case button_service::ButtonEvent::kLongPressUp:
        default:
            break;
    }

    ESP_LOGI(kTag, "Consumed wake-only POWER_OK event=%s",
             ButtonEventName(event.event));
    return true;
}

}  // namespace device_sleep_runtime
