#include "app_shell.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

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
#include "footer_runtime.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gemini_service.h"
#include "imu_service.h"
#include "lock_screen_runtime.h"
#include "overlay_runtime.h"
#include "power_service.h"
#include "project_assets.h"
#include "recording_session_service.h"
#include "recording_service.h"
#include "sdkconfig.h"
#include "status_bar_runtime.h"
#include "storage_service.h"
#include "timezone_service.h"
#include "touch_service.h"
#include "transcription_service.h"
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
std::atomic<bool> s_power_button_display_wake_only_active = false;
std::atomic<bool> s_startup_complete = false;
std::atomic<bool> s_gemini_ready = false;
std::atomic<bool> s_up_button_pressed = false;
std::mutex s_recording_session_feedback_mutex;
recording_session_service::Phase s_last_recording_session_feedback_phase =
    recording_session_service::Phase::kIdle;
std::string s_last_recording_session_feedback_status = {};
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

recording_session_service::Context BuildRecordingSessionContext()
{
    recording_session_service::Context context = {};
    context.lock_screen_active = lock_screen_runtime::IsActive();
    context.overlay_visible =
        overlay_runtime::IsShutdownModalVisible() || overlay_runtime::IsSelectModalVisible();
    return context;
}

epaper_ui::SelectModalState BuildRecordingTagSelectModalState()
{
    epaper_ui::SelectModalState state = {};
    state.visible = true;
    state.title_text = "Save recording as";
    state.selected_index = 0;
    for (const auto& option : recording_session_service::TagOptions()) {
        state.items.push_back({
            .label_text = std::string(option.label_text),
        });
    }
    return state;
}

epaper_ui::ToastState BuildToast(const char* text, EmbeddedIconId icon)
{
    epaper_ui::ToastState state = {};
    state.visible = true;
    state.body_text = text != nullptr ? text : "";
    state.leading_icon = project_assets::GetIcon(icon);
    return state;
}

bool IsShutdownPending(void*)
{
    return overlay_runtime::IsShutdownPending();
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

    recording_session_service::HandleRecordingEvent(event);

    const esp_err_t footer_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? footer_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer update after recording event failed: %s",
                 esp_err_to_name(footer_err));
    }
}

void HandleTranscriptionEvent(const transcription_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Transcription intent: ready=%d in_flight=%d http=%d status=%s error=%s chars=%u",
             event.snapshot.provider_ready ? 1 : 0,
             event.snapshot.request_in_flight ? 1 : 0,
             event.snapshot.last_http_status,
             event.snapshot.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.last_status_message.c_str(),
             event.snapshot.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.last_error_code.c_str(),
             static_cast<unsigned>(event.snapshot.last_transcript.size()));
    recording_session_service::HandleTranscriptionEvent(event);
}

void HandleRecordingSessionEvent(const recording_session_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Recording session: phase=%s allowed=%d clip_saved=%d transcript_saved=%d in_flight=%d status=%s error=%s",
             recording_session_service::PhaseName(event.snapshot.phase),
             event.snapshot.allowed ? 1 : 0,
             event.snapshot.clip_saved ? 1 : 0,
             event.snapshot.transcript_saved ? 1 : 0,
             event.snapshot.request_in_flight ? 1 : 0,
             event.snapshot.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.last_status_message.c_str(),
             event.snapshot.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.last_error_code.c_str());

    bool should_emit_feedback = false;
    {
        std::lock_guard<std::mutex> lock(s_recording_session_feedback_mutex);
        should_emit_feedback =
            event.snapshot.phase != s_last_recording_session_feedback_phase ||
            event.snapshot.last_status_message != s_last_recording_session_feedback_status;
        if (should_emit_feedback) {
            s_last_recording_session_feedback_phase = event.snapshot.phase;
            s_last_recording_session_feedback_status = event.snapshot.last_status_message;
        }
    }
    if (!should_emit_feedback) {
        return;
    }

    switch (event.snapshot.phase) {
        case recording_session_service::Phase::kAwaitingTagSelection: {
            const esp_err_t err =
                overlay_runtime::ShowSelectModal(BuildRecordingTagSelectModalState());
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording select modal failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kTranscribing: {
            const esp_err_t err = overlay_runtime::ShowToast(
                BuildToast("Transcribing recording...", EmbeddedIconId::kTranscribe));
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show transcription toast failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kComplete: {
            epaper_ui::ToastState toast = {};
            if (event.snapshot.transcript_saved) {
                toast = BuildToast("Transcript saved to SD", EmbeddedIconId::kFileTranscript);
            } else if (event.snapshot.clip_saved) {
                toast = BuildToast("Recording saved to SD", EmbeddedIconId::kCheck);
            } else {
                toast = BuildToast(event.snapshot.last_status_message.c_str(), EmbeddedIconId::kClose);
            }
            const esp_err_t err = overlay_runtime::ShowToastForDuration(toast, 2500);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording completion toast failed: %s",
                         esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kFailed: {
            const char* text = event.snapshot.last_status_message.empty()
                                   ? "Recording failed"
                                   : event.snapshot.last_status_message.c_str();
            const esp_err_t err = overlay_runtime::ShowToastForDuration(
                BuildToast(text, EmbeddedIconId::kClose), 2500);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording failure toast failed: %s",
                         esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kIdle:
        case recording_session_service::Phase::kArmed:
        case recording_session_service::Phase::kRecording:
        case recording_session_service::Phase::kSaving:
        default:
            break;
    }
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
    }

    const overlay_runtime::InputResult overlay_result =
        overlay_runtime::HandleButtonEvent(event);
    if (overlay_result.select_modal_submitted) {
        (void)recording_session_service::SubmitTagSelection(
            overlay_result.select_modal_selected_index);
    }
    if (overlay_result.request_shutdown && s_shutdown_task != nullptr) {
        xTaskNotifyGive(s_shutdown_task);
    }
    if (overlay_result.consumed) {
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

        ESP_LOGW(kTag, "Shutdown chord detected: UP held while POWER_OK pressed");
        const esp_err_t err = overlay_runtime::ShowShutdownModal();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show shutdown modal failed: %s", esp_err_to_name(err));
        }
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

    if (event.button == button_service::ButtonId::kPowerOk) {
        const recording_session_service::Context recording_context =
            BuildRecordingSessionContext();
        bool handled = false;
        switch (event.event) {
            case button_service::ButtonEvent::kPressDown:
                handled = recording_session_service::HandlePowerPressDown(recording_context);
                break;
            case button_service::ButtonEvent::kLongPressStart:
                handled = recording_session_service::HandlePowerLongPressStart(recording_context);
                break;
            case button_service::ButtonEvent::kPressUp:
                handled = recording_session_service::HandlePowerPressUp(recording_context);
                break;
            default:
                break;
        }
        if (handled) {
            return;
        }
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
            } else if (event.button != button_service::ButtonId::kDown) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonDoubleClick);
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
    if (event.count == 0) {
        s_touch_contact_active = false;
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
        const overlay_runtime::InputResult overlay_result =
            overlay_runtime::HandleTouchEvent(event);
        if (overlay_result.select_modal_submitted) {
            (void)recording_session_service::SubmitTagSelection(
                overlay_result.select_modal_selected_index);
        }
        if (overlay_result.request_shutdown && s_shutdown_task != nullptr) {
            xTaskNotifyGive(s_shutdown_task);
        }
        if (overlay_result.consumed) {
            return;
        }
        if (storage_service::IsWriteBusy()) {
            return;
        }
        if (new_contact) {
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
            overlay_runtime::SetShutdownRequestInProgress(false);
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
            overlay_runtime::SetShutdownRequestInProgress(false);
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

void InitOverlayRuntime()
{
    const esp_err_t err = overlay_runtime::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Overlay runtime init failed: %s", esp_err_to_name(err));
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

void InitTranscriptionService()
{
    transcription_service::SetEventHandler(HandleTranscriptionEvent, nullptr);
    const esp_err_t err = transcription_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Transcription service init failed: %s", esp_err_to_name(err));
    }
}

void InitRecordingSessionService()
{
    recording_session_service::SetEventHandler(HandleRecordingSessionEvent, nullptr);
    const esp_err_t err = recording_session_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Recording session service init failed: %s", esp_err_to_name(err));
    }
}

void InitFooterRuntime()
{
    epaper_ui::GlobalFooterState base_state = {};
    base_state.visible = true;
    footer_runtime::SetBaseState(base_state);
    const esp_err_t err = footer_runtime::UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer runtime init failed: %s", esp_err_to_name(err));
    }
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
    InitOverlayRuntime();
    PlayFeedback(feedback_service::FeedbackEvent::kStartup);
    InitTouchService();
    InitImuService();
    InitEnvironmentService();
    InitDeviceSleepRuntime();
    InitTimezoneService();
    InitGeminiService();
    InitWifiService();
    InitRecordingService();
    InitTranscriptionService();
    InitRecordingSessionService();
    InitFooterRuntime();
    StartShutdownTask();
    InitButtonService();
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial status bar update failed: %s", esp_err_to_name(status_bar_err));
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial footer update failed: %s", esp_err_to_name(footer_err));
    }
    RequestDemoSelection(display_service::DemoSelection::kTop,
                         display_service::RefreshMode::kFull,
                         "startup_complete");
    s_startup_complete.store(true, std::memory_order_relaxed);
}

}  // namespace app_shell
