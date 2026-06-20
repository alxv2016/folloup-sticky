#include "app_shell.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

#include "buzzer_service.h"
#include "button_service.h"
#include "device_sleep_runtime.h"
#include "display_service.h"
#include "environment_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_service.h"
#include "power_service.h"
#include "recording_service.h"
#include "sdkconfig.h"
#include "storage_service.h"
#include "timezone_service.h"
#include "touch_service.h"
#include "wifi_service.h"

namespace app_shell {
namespace {

constexpr const char* kTag = "AppShell";
constexpr bool kEnablePowerButtonShutdown = true;
constexpr uint32_t kAutoSleepDisplaySleepTimeoutSeconds =
    CONFIG_FOLLOWUP_AUTO_SLEEP_DISPLAY_SLEEP_TIMEOUT_SECONDS;
constexpr uint32_t kAutoSleepLightSleepTimeoutSeconds =
    CONFIG_FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS;
constexpr uint32_t kShutdownTaskStackWords = 3072;
constexpr UBaseType_t kShutdownTaskPriority = 5;
constexpr TickType_t kPowerButtonReleaseSettleDelay = pdMS_TO_TICKS(500);
constexpr TickType_t kTouchContactGap = pdMS_TO_TICKS(300);
constexpr const char* kMicDemoWavName = "mic_demo.wav";

TaskHandle_t s_shutdown_task = nullptr;
std::atomic<bool> s_power_button_shutdown_pending = false;
bool s_touch_contact_active = false;
TickType_t s_last_touch_event_tick = 0;

void PlayBuzzerPattern(buzzer_service::Pattern pattern, const char* name)
{
    const esp_err_t err = buzzer_service::PlayPattern(pattern);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Buzzer %s pattern failed: %s", name, esp_err_to_name(err));
    }
}

void RequestDemoSelection(display_service::DemoSelection selection, const char* source)
{
    const esp_err_t err = display_service::SelectDemoSelection(selection);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Display demo selection from %s failed: %s",
                 source, esp_err_to_name(err));
        return;
    }
}

const char* DemoActionName()
{
    return "format_sd";
}

bool IsStorageActionBlocked()
{
    const storage_service::Snapshot snapshot = storage_service::GetSnapshot();
    return snapshot.mode != storage_service::Mode::kAppMounted ||
           storage_service::IsWriteBusy();
}

void ActivateSelectedDemoAction()
{
    ESP_LOGI(kTag, "Activating selected demo action: %s", DemoActionName());
    if (IsStorageActionBlocked()) {
        const storage_service::Snapshot snapshot = storage_service::GetSnapshot();
        ESP_LOGW(kTag, "Storage action ignored while mode=%s write_busy=%d",
                 storage_service::ModeName(snapshot.mode),
                 storage_service::IsWriteBusy() ? 1 : 0);
        return;
    }

    const esp_err_t err = storage_service::RequestFormatSdCard();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Format SD request failed: %s", esp_err_to_name(err));
    }
}

void ConfirmPendingOtaImage()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;

    if (running != nullptr &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}

const char* ButtonIdName(button_service::ButtonId button)
{
    switch (button) {
        case button_service::ButtonId::kPowerOk:
            return "POWER_OK";
        case button_service::ButtonId::kUp:
            return "UP";
        case button_service::ButtonId::kDown:
            return "DOWN";
        default:
            return "UNKNOWN";
    }
}

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

const char* RecordingStateName(recording_service::State state)
{
    switch (state) {
        case recording_service::State::kIdle:
            return "IDLE";
        case recording_service::State::kArmed:
            return "ARMED";
        case recording_service::State::kRecording:
            return "RECORDING";
        case recording_service::State::kClipReady:
            return "CLIP_READY";
        default:
            return "UNKNOWN";
    }
}

bool IsShutdownPending(void*)
{
    return s_power_button_shutdown_pending.load(std::memory_order_relaxed);
}

void HandleRecordingEvent(const recording_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Recording intent: state=%s armed=%d recording=%d has_clip=%d saving=%d exporting=%d samples=%u duration_ms=%lu level=%u",
             RecordingStateName(event.state),
             event.ui_state.armed ? 1 : 0,
             event.ui_state.recording ? 1 : 0,
             event.ui_state.has_clip ? 1 : 0,
             event.ui_state.saving ? 1 : 0,
             event.ui_state.exporting ? 1 : 0,
             static_cast<unsigned>(event.ui_state.recorded_samples),
             static_cast<unsigned long>(event.ui_state.duration_ms),
             static_cast<unsigned>(event.ui_state.input_level_percent));
}

void HandleStorageEvent(const storage_service::Event& event, void*)
{
    ESP_LOGI(kTag, "Storage intent: mode=%s operation=%s phase=%s err=%s",
             storage_service::ModeName(event.snapshot.mode),
             storage_service::OperationName(event.snapshot.operation),
             storage_service::OperationPhaseName(event.snapshot.phase),
             esp_err_to_name(event.snapshot.last_error));
}

void HandleTimezoneEvent(const timezone_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Time intent: enabled=%d timezone=%s valid=%d source=%s ntp_synced=%d syncing=%d date=%s time=%s",
             event.snapshot.settings.enabled ? 1 : 0,
             event.snapshot.settings.timezone_name.empty()
                 ? "<unset>"
                 : event.snapshot.settings.timezone_name.c_str(),
             event.snapshot.runtime.time_valid ? 1 : 0,
             timezone_service::TimeSourceName(event.snapshot.runtime.time_source),
             event.snapshot.runtime.has_network_sync ? 1 : 0,
             event.snapshot.runtime.sync_in_progress ? 1 : 0,
             event.snapshot.runtime.current_date.empty()
                 ? "--"
                 : event.snapshot.runtime.current_date.c_str(),
             event.snapshot.runtime.current_time.empty()
                 ? "--:--"
                 : event.snapshot.runtime.current_time.c_str());
}

void RegisterWifiBackendRoutes(httpd_handle_t server, void*)
{
    timezone_service::RegisterPortalRoutes(server);
}

void HandleWifiEvent(const wifi_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Wi-Fi intent: state=%s detail=%s enabled=%d connected=%d ap=%d ssid=%s ip=%s ap_ssid=%s ap_url=%s rssi=%d",
             wifi_service::StateName(event.state),
             event.detail.empty() ? "" : event.detail.c_str(),
             event.ui_state.wifi_enabled ? 1 : 0,
             event.ui_state.connected ? 1 : 0,
             event.ui_state.access_point_mode ? 1 : 0,
             event.ui_state.ssid.empty() ? "<none>" : event.ui_state.ssid.c_str(),
             event.ui_state.ip_address.empty() ? "<none>" : event.ui_state.ip_address.c_str(),
             event.ui_state.ap_ssid.empty() ? "<none>" : event.ui_state.ap_ssid.c_str(),
             event.ui_state.ap_url.empty() ? "<none>" : event.ui_state.ap_url.c_str(),
             event.ui_state.rssi);
    timezone_service::SetNetworkConnected(event.ui_state.connected);
}

void HandleButtonEvent(const button_service::ButtonEventInfo& event, void*)
{
    ESP_LOGI(kTag, "Button intent: button=%s event=%s pressed_ms=%lu",
             ButtonIdName(event.button), ButtonEventName(event.event),
             static_cast<unsigned long>(event.pressed_ms));
    if (device_sleep_runtime::ConsumeWakeOnlyPowerButtonEvent(event)) {
        return;
    }

    if (storage_service::IsWriteBusy()) {
        ESP_LOGI(kTag, "Button ignored while storage write is active");
        return;
    }

    device_sleep_runtime::NotifyUserActivity();

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            PlayBuzzerPattern(buzzer_service::Pattern::kClick, "click");
            if (event.button == button_service::ButtonId::kUp ||
                event.button == button_service::ButtonId::kDown) {
                RequestDemoSelection(display_service::DemoSelection::kTop, "button");
            }
            break;
        case button_service::ButtonEvent::kDoubleClick:
            PlayBuzzerPattern(buzzer_service::Pattern::kDoubleClick, "double-click");
            if (event.button == button_service::ButtonId::kDown) {
                ActivateSelectedDemoAction();
            }
            break;
        case button_service::ButtonEvent::kLongPressStart:
            PlayBuzzerPattern(buzzer_service::Pattern::kLongClick, "long-click");
            break;
        default:
            break;
    }

    if (event.button != button_service::ButtonId::kPowerOk) {
        return;
    }

    if (event.event == button_service::ButtonEvent::kLongPressStart) {
        s_power_button_shutdown_pending.store(true, std::memory_order_relaxed);
        ESP_LOGW(kTag, "Power button long press detected; release to shutdown");
        return;
    }

    if (event.event != button_service::ButtonEvent::kLongPressUp ||
        !s_power_button_shutdown_pending.load(std::memory_order_relaxed)) {
        return;
    }

    s_power_button_shutdown_pending.store(false, std::memory_order_relaxed);

    if constexpr (!kEnablePowerButtonShutdown) {
        ESP_LOGW(kTag, "Power button released after long press; shutdown is disabled");
        return;
    }

    if (s_shutdown_task == nullptr) {
        ESP_LOGW(kTag, "Power button released after long press; shutdown task unavailable");
        return;
    }

    ESP_LOGW(kTag, "Power button released after long press; requesting shutdown");
    xTaskNotifyGive(s_shutdown_task);
}

void HandleTouchEvent(const touch_service::TouchEventInfo& event, void*)
{
    ESP_LOGD(kTag, "Touch intent: count=%u", static_cast<unsigned>(event.count));
    if (storage_service::IsWriteBusy()) {
        return;
    }
    if (event.count > 0) {
        device_sleep_runtime::NotifyUserActivity();

        const TickType_t now = xTaskGetTickCount();
        const bool new_contact =
            !s_touch_contact_active || now - s_last_touch_event_tick >= kTouchContactGap;
        s_touch_contact_active = true;
        s_last_touch_event_tick = now;

        if (new_contact) {
            RequestDemoSelection(display_service::DemoSelection::kTop, "touch");
            PlayBuzzerPattern(buzzer_service::Pattern::kClick, "touch");
        }
    }

    for (uint8_t i = 0; i < event.count; ++i) {
        ESP_LOGD(kTag, "Touch intent point[%u]: x=%u y=%u size=%u id=%u",
                 static_cast<unsigned>(i), static_cast<unsigned>(event.points[i].x),
                 static_cast<unsigned>(event.points[i].y),
                 static_cast<unsigned>(event.points[i].size),
                 static_cast<unsigned>(event.points[i].id));
    }
}

void ShutdownTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(kTag, "Shutdown request accepted; waiting for power button release settle");
        PlayBuzzerPattern(buzzer_service::Pattern::kShutdown, "shutdown");
        vTaskDelay(kPowerButtonReleaseSettleDelay);
        ESP_LOGW(kTag, "Power button release settled; releasing power hold");
        const esp_err_t err = power_service::RequestShutdown();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Shutdown request failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGW(kTag, "Shutdown request returned; board may still be powered");
        }
    }
}

void StartShutdownTask()
{
    if (s_shutdown_task != nullptr) {
        return;
    }

    const BaseType_t created = xTaskCreate(ShutdownTask, "app_shutdown",
                                          kShutdownTaskStackWords, nullptr,
                                          kShutdownTaskPriority,
                                          &s_shutdown_task);
    if (created != pdPASS) {
        s_shutdown_task = nullptr;
        ESP_LOGW(kTag, "Failed to create shutdown task");
    }
}

void InitButtonService()
{
    button_service::SetEventHandler(HandleButtonEvent, nullptr);
    const esp_err_t err = button_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Button service init failed: %s", esp_err_to_name(err));
    }
}

void InitBuzzerService()
{
    const esp_err_t err = buzzer_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Buzzer service init failed: %s", esp_err_to_name(err));
        return;
    }

    const esp_err_t play_err =
        buzzer_service::PlayPattern(buzzer_service::Pattern::kStartup);
    if (play_err != ESP_OK) {
        ESP_LOGW(kTag, "Buzzer startup pattern failed: %s",
                 esp_err_to_name(play_err));
    }
}

void InitDisplayService()
{
    const esp_err_t err = display_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Display service init failed: %s", esp_err_to_name(err));
    }
}

void InitStorageService()
{
    storage_service::SetEventHandler(HandleStorageEvent, nullptr);
    const esp_err_t err = storage_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Storage service init failed: %s", esp_err_to_name(err));
    }
    storage_service::LogDebugStatus();
}

void InitTimezoneService()
{
    timezone_service::SetEventHandler(HandleTimezoneEvent, nullptr);
    const esp_err_t err = timezone_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Timezone service init failed: %s", esp_err_to_name(err));
    }
}

void InitWifiService()
{
    wifi_service::SetEventHandler(HandleWifiEvent, nullptr);
    wifi_service::SetPortalRouteRegistrar(RegisterWifiBackendRoutes, nullptr);
    const esp_err_t err = wifi_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Wi-Fi service init failed: %s", esp_err_to_name(err));
        return;
    }
    wifi_service::Start();
}

void InitRecordingService()
{
    recording_service::SetEventHandler(HandleRecordingEvent, nullptr);
    const esp_err_t err = recording_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Recording service init failed: %s", esp_err_to_name(err));
        return;
    }

    recording_service::LogDebugStatus();
    if (!storage_service::IsMounted()) {
        ESP_LOGW(kTag, "Storage not mounted; skipping mic WAV capture demo");
        return;
    }

    char wav_path[96] = {};
    std::snprintf(wav_path, sizeof(wav_path), "%s/%s",
                  storage_service::MountPoint(), kMicDemoWavName);
    const esp_err_t capture_err = recording_service::RecordDebugClipToWav(wav_path);
    if (capture_err != ESP_OK) {
        ESP_LOGW(kTag, "Mic WAV capture demo failed: %s", esp_err_to_name(capture_err));
    }
    recording_service::LogDebugStatus();
}

void InitTouchService()
{
    touch_service::SetEventHandler(HandleTouchEvent, nullptr);
    const esp_err_t err = touch_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Touch service init failed: %s", esp_err_to_name(err));
    }
}

void InitImuService()
{
    const esp_err_t err = imu_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "IMU service init failed: %s", esp_err_to_name(err));
        return;
    }
    imu_service::LogDebugStatus();
}

void InitEnvironmentService()
{
    const esp_err_t err = environment_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Environment service init failed: %s", esp_err_to_name(err));
        return;
    }
    environment_service::LogDebugStatus();
}

void InitDeviceSleepRuntime()
{
    device_sleep_runtime::SetShutdownPendingProvider(IsShutdownPending, nullptr);

    device_sleep_runtime::AutoSleepSettings settings = {};
    settings.enabled = true;
    settings.display_sleep_timeout_seconds = kAutoSleepDisplaySleepTimeoutSeconds;
    settings.light_sleep_timeout_seconds = kAutoSleepLightSleepTimeoutSeconds;
    settings.motion_wake_enabled = true;
    settings.interaction_wake_enabled = true;

    const esp_err_t err = device_sleep_runtime::StartAutoSleep(settings);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Device auto-sleep init failed: %s", esp_err_to_name(err));
    }

    const esp_err_t motion_err = device_sleep_runtime::StartMotionPolling();
    if (motion_err != ESP_OK) {
        ESP_LOGW(kTag, "Device sleep motion polling init failed: %s",
                 esp_err_to_name(motion_err));
    }
}

}  // namespace

void Run()
{
    ESP_ERROR_CHECK(power_service::EnablePowerHold());
    ConfirmPendingOtaImage();
    ESP_ERROR_CHECK(power_service::Init());
    power_service::LogDebugStatus();
    InitBuzzerService();
    InitDisplayService();
    InitTouchService();
    InitImuService();
    InitEnvironmentService();
    InitDeviceSleepRuntime();
    InitTimezoneService();
    InitWifiService();
    InitStorageService();
    InitRecordingService();
    StartShutdownTask();
    InitButtonService();
}

}  // namespace app_shell
