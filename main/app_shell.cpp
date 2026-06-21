#include "app_shell.h"

#include <atomic>
#include <cstdint>
#include <cstring>

#include "button_service.h"
#include "device_sleep_service.h"
#include "device_sleep_runtime.h"
#include "display_service.h"
#include "environment_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "feedback_service.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gemini_service.h"
#include "imu_service.h"
#include "lock_screen_runtime.h"
#include "power_service.h"
#include "recording_service.h"
#include "sdkconfig.h"
#include "status_bar_runtime.h"
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
constexpr TickType_t kPowerButtonReleaseSettleDelay = pdMS_TO_TICKS(500);
constexpr TickType_t kTouchFeedbackContactGap = pdMS_TO_TICKS(300);

TaskHandle_t s_shutdown_task = nullptr;
std::atomic<bool> s_shutdown_request_in_progress = false;
std::atomic<bool> s_power_button_display_wake_only_active = false;
std::atomic<bool> s_startup_complete = false;
std::atomic<bool> s_gemini_ready = false;
std::atomic<bool> s_up_button_pressed = false;
std::atomic<bool> s_power_button_pressed = false;
std::atomic<bool> s_shutdown_combo_active = false;
bool s_touch_contact_active = false;
TickType_t s_last_touch_event_tick = 0;

void PlayFeedback(feedback_service::FeedbackEvent event)
{
    (void)feedback_service::Play(event);
}

void RequestDemoSelection(display_service::DemoSelection selection,
                          display_service::RefreshMode refresh_mode,
                          const char* source)
{
    const bool startup_complete = s_startup_complete.load(std::memory_order_relaxed);
    const bool startup_handoff = source != nullptr &&
                                 std::strcmp(source, "startup_complete") == 0;
    if (!startup_complete && !startup_handoff) {
        return;
    }

    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update before display refresh from %s failed: %s",
                 source, esp_err_to_name(status_bar_err));
    }

    const esp_err_t err = display_service::SelectDemoSelection(selection, refresh_mode);
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
    return s_shutdown_request_in_progress.load(std::memory_order_relaxed);
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

    const esp_err_t lock_screen_err = lock_screen_runtime::SyncClockState(true);
    if (lock_screen_err != ESP_OK && lock_screen_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Lock screen clock update after time event failed: %s",
                 esp_err_to_name(lock_screen_err));
    }

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after time event failed: %s",
                 esp_err_to_name(status_bar_err));
    }
}

void RegisterWifiBackendRoutes(httpd_handle_t server, void*)
{
    timezone_service::RegisterPortalRoutes(server);
    gemini_service::RegisterPortalRoutes(server);
}

void HandleGeminiEvent(const gemini_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Gemini intent: configured=%d source=%s ready=%d auth_checked=%d in_flight=%d http=%d status=%s error=%s",
             event.snapshot.settings.configured ? 1 : 0,
             gemini_service::ApiKeySourceName(event.snapshot.settings.api_key_source),
             event.snapshot.runtime.ready ? 1 : 0,
             event.snapshot.runtime.auth_checked ? 1 : 0,
             event.snapshot.runtime.request_in_flight ? 1 : 0,
             event.snapshot.runtime.last_http_status,
             event.snapshot.runtime.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.runtime.last_status_message.c_str(),
             event.snapshot.runtime.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.runtime.last_error_code.c_str());

    const bool ready = event.snapshot.runtime.ready;
    const bool was_ready = s_gemini_ready.exchange(ready, std::memory_order_relaxed);
    if (ready && !was_ready) {
        PlayFeedback(feedback_service::FeedbackEvent::kGeminiConnected);
    }

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after Gemini event failed: %s",
                 esp_err_to_name(status_bar_err));
    }
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
    gemini_service::SetNetworkState(event.ui_state.connected,
                                    event.ui_state.access_point_mode);

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after Wi-Fi event failed: %s",
                 esp_err_to_name(status_bar_err));
    }
}

void HandleButtonEvent(const button_service::ButtonEventInfo& event, void*)
{
    ESP_LOGI(kTag, "Button intent: button=%s event=%s pressed_ms=%lu",
             ButtonIdName(event.button), ButtonEventName(event.event),
             static_cast<unsigned long>(event.pressed_ms));

    if (event.button == button_service::ButtonId::kUp) {
        if (event.event == button_service::ButtonEvent::kPressDown) {
            s_up_button_pressed.store(true, std::memory_order_relaxed);
        } else if (event.event == button_service::ButtonEvent::kPressUp) {
            s_up_button_pressed.store(false, std::memory_order_relaxed);
        }
    } else if (event.button == button_service::ButtonId::kPowerOk) {
        if (event.event == button_service::ButtonEvent::kPressDown) {
            s_power_button_pressed.store(true, std::memory_order_relaxed);
        } else if (event.event == button_service::ButtonEvent::kPressUp) {
            s_power_button_pressed.store(false, std::memory_order_relaxed);
        }
    }

    if (s_shutdown_combo_active.load(std::memory_order_relaxed) &&
        (event.button == button_service::ButtonId::kUp ||
         event.button == button_service::ButtonId::kPowerOk)) {
        if (!s_up_button_pressed.load(std::memory_order_relaxed) &&
            !s_power_button_pressed.load(std::memory_order_relaxed)) {
            s_shutdown_combo_active.store(false, std::memory_order_relaxed);
        }
        ESP_LOGI(kTag, "Consumed shutdown chord button=%s event=%s",
                 ButtonIdName(event.button), ButtonEventName(event.event));
        return;
    }

    if (device_sleep_runtime::ConsumeWakeOnlyPowerButtonEvent(event)) {
        return;
    }

    if (event.button == button_service::ButtonId::kPowerOk &&
        s_power_button_display_wake_only_active.load(std::memory_order_relaxed)) {
        ESP_LOGI(kTag, "Consumed display-wake POWER_OK event=%s",
                 ButtonEventName(event.event));
        switch (event.event) {
            case button_service::ButtonEvent::kPressUp:
            case button_service::ButtonEvent::kSingleClick:
            case button_service::ButtonEvent::kDoubleClick:
            case button_service::ButtonEvent::kLongPressUp:
                s_power_button_display_wake_only_active.store(false,
                                                              std::memory_order_relaxed);
                break;
            case button_service::ButtonEvent::kPressDown:
            case button_service::ButtonEvent::kLongPressStart:
            default:
                break;
        }
        return;
    }

    if (storage_service::IsWriteBusy()) {
        ESP_LOGI(kTag, "Button ignored while storage write is active");
        return;
    }

    if (event.button == button_service::ButtonId::kPowerOk &&
        event.event == button_service::ButtonEvent::kPressDown &&
        s_up_button_pressed.load(std::memory_order_relaxed)) {
        if constexpr (!kEnablePowerButtonShutdown) {
            ESP_LOGW(kTag, "Shutdown chord detected but shutdown is disabled");
            return;
        }

        if (s_shutdown_task == nullptr) {
            ESP_LOGW(kTag, "Shutdown chord detected but shutdown task unavailable");
            return;
        }

        s_shutdown_request_in_progress.store(true, std::memory_order_relaxed);
        s_shutdown_combo_active.store(true, std::memory_order_relaxed);
        ESP_LOGW(kTag, "Shutdown chord detected: UP held while POWER_OK pressed");
        xTaskNotifyGive(s_shutdown_task);
        return;
    }

    const device_sleep_service::Stage stage_before =
        device_sleep_service::GetSnapshot().runtime.stage;
    device_sleep_runtime::NotifyUserActivity();
    if (event.button == button_service::ButtonId::kPowerOk &&
        stage_before == device_sleep_service::Stage::kDisplaySleeping) {
        s_power_button_display_wake_only_active.store(true, std::memory_order_relaxed);
        ESP_LOGI(kTag, "POWER_OK armed as wake-only for display sleep");
        return;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            if (event.button == button_service::ButtonId::kUp) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonClick);
                if (lock_screen_runtime::IsActive()) {
                    const esp_err_t err = display_service::RequestRefreshCurrentScreen(
                        display_service::RefreshMode::kPartial);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(kTag, "Lock screen partial refresh failed: %s",
                                 esp_err_to_name(err));
                    }
                } else {
                    RequestDemoSelection(display_service::DemoSelection::kTop,
                                         display_service::RefreshMode::kPartial,
                                         "button_up");
                }
            } else if (event.button == button_service::ButtonId::kDown) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonClick);
                if (lock_screen_runtime::IsActive()) {
                    const esp_err_t err = display_service::RequestRefreshCurrentScreen(
                        display_service::RefreshMode::kFull);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(kTag, "Lock screen full refresh failed: %s",
                                 esp_err_to_name(err));
                    }
                } else {
                    RequestDemoSelection(display_service::DemoSelection::kTop,
                                         display_service::RefreshMode::kFull,
                                         "button_down");
                }
            }
            break;
        case button_service::ButtonEvent::kDoubleClick:
            if (event.button == button_service::ButtonId::kPowerOk) {
                const bool was_active = lock_screen_runtime::IsActive();
                const esp_err_t err = lock_screen_runtime::Toggle();
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    ESP_LOGW(kTag, "Power button lock toggle failed: %s",
                             esp_err_to_name(err));
                } else {
                    PlayFeedback(was_active ? feedback_service::FeedbackEvent::kUnlock
                                            : feedback_service::FeedbackEvent::kLock);
                }
            } else {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonDoubleClick);
                if (!lock_screen_runtime::IsActive() &&
                    event.button == button_service::ButtonId::kDown) {
                    ActivateSelectedDemoAction();
                }
            }
            break;
        case button_service::ButtonEvent::kLongPressStart:
            if (event.button != button_service::ButtonId::kPowerOk) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonLongPress);
            }
            break;
        default:
            break;
    }

}

void HandleTouchEvent(const touch_service::TouchEventInfo& event, void*)
{
    ESP_LOGD(kTag, "Touch intent: count=%u", static_cast<unsigned>(event.count));
    if (!s_startup_complete.load(std::memory_order_relaxed)) {
        return;
    }
    if (storage_service::IsWriteBusy()) {
        return;
    }
    if (event.count > 0) {
        device_sleep_runtime::NotifyUserActivity();
        const TickType_t now = xTaskGetTickCount();
        const bool new_contact =
            !s_touch_contact_active ||
            now - s_last_touch_event_tick >= kTouchFeedbackContactGap;
        s_touch_contact_active = true;
        s_last_touch_event_tick = now;
        if (new_contact) {
            if (!lock_screen_runtime::IsActive()) {
                RequestDemoSelection(display_service::DemoSelection::kTop,
                                     display_service::RefreshMode::kPartial,
                                     "touch");
            }
            PlayFeedback(feedback_service::FeedbackEvent::kTouchContact);
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
        PlayFeedback(feedback_service::FeedbackEvent::kShutdown);
        vTaskDelay(kPowerButtonReleaseSettleDelay);
        status_bar_runtime::SetShutdownIndicatorVisible(true);
        const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayStateAndRefreshNow(
            display_service::RefreshMode::kPartial);
        if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Shutdown indicator refresh failed: %s",
                     esp_err_to_name(status_bar_err));
        }
        ESP_LOGW(kTag, "Power button release settled; releasing power hold");
        const esp_err_t err = power_service::RequestShutdown();
        if (err != ESP_OK) {
            s_shutdown_request_in_progress.store(false, std::memory_order_relaxed);
            status_bar_runtime::SetShutdownIndicatorVisible(false);
            const esp_err_t clear_err =
                status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                    display_service::RefreshMode::kPartial);
            if (clear_err != ESP_OK && clear_err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Shutdown indicator clear failed: %s",
                         esp_err_to_name(clear_err));
            }
            ESP_LOGW(kTag, "Shutdown request failed: %s", esp_err_to_name(err));
        } else {
            s_shutdown_request_in_progress.store(false, std::memory_order_relaxed);
            ESP_LOGW(kTag, "Shutdown request returned; board may still be powered");
        }
    }
}

void StartShutdownTask()
{
    if (s_shutdown_task != nullptr) {
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        ShutdownTask,
        "app_shutdown",
        kShutdownTaskStackWords,
        nullptr,
        followup_task_config::kPriorityAppShutdown,
        &s_shutdown_task,
        followup_task_config::kAppCore);
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

void InitFeedbackService()
{
    const esp_err_t err = feedback_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Feedback service init failed: %s", esp_err_to_name(err));
        return;
    }
}

void InitDisplayService()
{
    const esp_err_t err = display_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Display service init failed: %s", esp_err_to_name(err));
    }
}

void InitLockScreenRuntime()
{
    const esp_err_t err = lock_screen_runtime::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Lock screen runtime init failed: %s", esp_err_to_name(err));
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

void InitGeminiService()
{
    gemini_service::SetEventHandler(HandleGeminiEvent, nullptr);
    const esp_err_t err = gemini_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Gemini service init failed: %s", esp_err_to_name(err));
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
    InitFeedbackService();
    // On this board the SD card needs to enter and stay in SPI mode before
    // the shared-bus display path is brought up.
    InitStorageService();
    InitDisplayService();
    InitLockScreenRuntime();
    PlayFeedback(feedback_service::FeedbackEvent::kStartup);
    InitTouchService();
    InitImuService();
    InitEnvironmentService();
    InitDeviceSleepRuntime();
    InitTimezoneService();
    InitGeminiService();
    InitWifiService();
    InitRecordingService();
    StartShutdownTask();
    InitButtonService();
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial status bar update failed: %s", esp_err_to_name(status_bar_err));
    }
    RequestDemoSelection(display_service::DemoSelection::kTop,
                         display_service::RefreshMode::kFull,
                         "startup_complete");
    s_startup_complete.store(true, std::memory_order_relaxed);
}

}  // namespace app_shell
