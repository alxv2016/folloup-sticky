#include "device_sleep_runtime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>

#include "device_sleep_service.h"
#include "display_service.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "recording_service.h"
#include "sticky_board_config.h"
#include "storage_service.h"
#include "touch_service.h"

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
constexpr UBaseType_t kMotionTaskPriority = 3;
constexpr uint32_t kAutoSleepTaskStackWords = 4096;
constexpr UBaseType_t kAutoSleepTaskPriority = 4;
constexpr size_t kAutoSleepEventQueueDepth = 8;

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
std::atomic<bool> s_power_button_wake_only_active = false;
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

esp_err_t EnterLightSleep()
{
    ESP_RETURN_ON_ERROR(display_service::EnterLightSleep(),
                        kTag,
                        "display light sleep message failed");

    gpio_config_t power_button_config = {};
    power_button_config.pin_bit_mask = 1ULL << STICKY_POWER_BUTTON_PIN;
    power_button_config.mode = GPIO_MODE_INPUT;
    power_button_config.pull_up_en = GPIO_PULLUP_ENABLE;
    power_button_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_button_config.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&power_button_config),
                        kTag,
                        "configure POWER_OK light-sleep wake GPIO failed");

    ESP_RETURN_ON_ERROR(gpio_wakeup_enable(STICKY_POWER_BUTTON_PIN, GPIO_INTR_LOW_LEVEL),
                        kTag,
                        "enable POWER_OK GPIO wake failed");
    ESP_RETURN_ON_ERROR(esp_sleep_enable_gpio_wakeup(),
                        kTag,
                        "enable GPIO light-sleep wake failed");

    ESP_LOGI(kTag, "Entering light sleep with POWER_OK wake");
    const esp_err_t sleep_err = esp_light_sleep_start();
    const esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    const bool power_button_wake = gpio_get_level(STICKY_POWER_BUTTON_PIN) == 0;

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    gpio_wakeup_disable(STICKY_POWER_BUTTON_PIN);

    ESP_LOGI(kTag, "Exited light sleep: cause=%s power_button=%d err=%s",
             WakeupCauseName(wakeup_cause),
             power_button_wake ? 1 : 0,
             esp_err_to_name(sleep_err));

    if (power_button_wake) {
        s_power_button_wake_only_active.store(true, std::memory_order_relaxed);
    }

    device_sleep_service::NotifyUserActivity(device_sleep_service::ActivitySource::kInteraction);
    return sleep_err;
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
            err = display_service::EnterDisplaySleep();
            break;
        case device_sleep_service::Action::kWakeDisplay:
            err = display_service::WakeDisplay();
            break;
        case device_sleep_service::Action::kEnterLightSleep:
            err = EnterLightSleep();
            break;
        case device_sleep_service::Action::kWakeFromLightSleep:
            err = display_service::WakeDisplay();
            if (touch_service::IsInitialized()) {
                const esp_err_t touch_err = touch_service::RecoverAfterLightSleep();
                if (touch_err != ESP_OK) {
                    ESP_LOGW(kTag, "Touch recovery after light sleep failed: %s",
                             esp_err_to_name(touch_err));
                }
            }
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

    const BaseType_t created = xTaskCreate(AutoSleepTask,
                                          "app_sleep",
                                          kAutoSleepTaskStackWords,
                                          nullptr,
                                          kAutoSleepTaskPriority,
                                          &s_auto_sleep_task);
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

    const BaseType_t created = xTaskCreate(MotionPollingTask,
                                          "sleep_motion",
                                          kMotionTaskStackWords,
                                          nullptr,
                                          kMotionTaskPriority,
                                          &s_motion_task);
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

bool ConsumeWakeOnlyPowerButtonEvent(const button_service::ButtonEventInfo& event)
{
    if (event.button != button_service::ButtonId::kPowerOk ||
        !s_power_button_wake_only_active.load(std::memory_order_relaxed)) {
        return false;
    }

    ESP_LOGI(kTag, "Consumed wake-only POWER_OK event=%s",
             ButtonEventName(event.event));

    switch (event.event) {
        case button_service::ButtonEvent::kPressUp:
        case button_service::ButtonEvent::kSingleClick:
        case button_service::ButtonEvent::kDoubleClick:
        case button_service::ButtonEvent::kLongPressUp:
            s_power_button_wake_only_active.store(false, std::memory_order_relaxed);
            break;
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kLongPressStart:
        default:
            break;
    }

    return true;
}

}  // namespace device_sleep_runtime
