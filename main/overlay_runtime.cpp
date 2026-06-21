#include "overlay_runtime.h"

#include <mutex>

#include "display_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "feedback_service.h"

namespace overlay_runtime {
namespace {

constexpr const char* kTag = "OverlayRuntime";
constexpr uint64_t kTouchActionCooldownMs = 350;
constexpr uint64_t kTouchFeedbackCooldownMs = 250;

std::mutex s_state_mutex;
bool s_initialized = false;
epaper_ui::ShutdownModalState s_shutdown_modal_state = {};
epaper_ui::ToastState s_toast_state = {};
bool s_shutdown_request_in_progress = false;
uint64_t s_touch_action_cooldown_until_ms = 0;
uint64_t s_touch_feedback_cooldown_until_ms = 0;

uint64_t MonotonicMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

bool HitShutdownActionAnyOrientation(int portrait_width,
                                     int portrait_height,
                                     int x,
                                     int y,
                                     epaper_ui::ShutdownModalAction* action)
{
    bool hit = false;
    epaper_ui::ShutdownModalAction candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, x, y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    const int mirrored_x = std::max(0, portrait_width - 1 - x);
    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, mirrored_x, y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    const int mirrored_y = std::max(0, portrait_height - 1 - y);
    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, x, mirrored_y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, mirrored_x, mirrored_y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    return false;
}

esp_err_t ApplyOverlayState(const epaper_ui::ShutdownModalState& shutdown_modal_state,
                            const epaper_ui::ToastState& toast_state,
                            bool request_refresh)
{
    ESP_RETURN_ON_ERROR(display_service::SetShutdownModalState(shutdown_modal_state),
                        kTag,
                        "set shutdown modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetToastState(toast_state),
                        kTag,
                        "set toast state failed");
    if (!request_refresh) {
        return ESP_OK;
    }
    return display_service::RequestRefreshCurrentScreen(display_service::RefreshMode::kPartial);
}

esp_err_t SyncOverlayState(bool request_refresh)
{
    epaper_ui::ShutdownModalState shutdown_modal_state = {};
    epaper_ui::ToastState toast_state = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        shutdown_modal_state = s_shutdown_modal_state;
        toast_state = s_toast_state;
    }

    return ApplyOverlayState(shutdown_modal_state, toast_state, request_refresh);
}

}  // namespace

esp_err_t Init()
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (s_initialized) {
            return ESP_OK;
        }
        s_shutdown_modal_state = {};
        s_toast_state = {};
        s_shutdown_request_in_progress = false;
        s_initialized = true;
    }

    ESP_RETURN_ON_ERROR(SyncOverlayState(false), kTag, "initial overlay sync failed");
    ESP_LOGI(kTag, "Overlay runtime initialized");
    return ESP_OK;
}

bool IsShutdownModalVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible;
}

bool IsShutdownPending()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress;
}

bool IsInputCaptured()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress;
}

esp_err_t ShowShutdownModal()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!s_shutdown_modal_state.visible ||
            s_shutdown_modal_state.selected_action != epaper_ui::ShutdownModalAction::kCancel) {
            s_shutdown_modal_state.visible = true;
            s_shutdown_modal_state.selected_action = epaper_ui::ShutdownModalAction::kCancel;
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Shutdown modal shown");
    (void)feedback_service::Play(feedback_service::FeedbackEvent::kButtonClick);
    return SyncOverlayState(true);
}

esp_err_t DismissShutdownModal()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        if (s_shutdown_modal_state.visible) {
            s_shutdown_modal_state.visible = false;
            s_shutdown_modal_state.selected_action = epaper_ui::ShutdownModalAction::kCancel;
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Shutdown modal dismissed");
    return SyncOverlayState(true);
}

void SetShutdownRequestInProgress(bool in_progress)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_shutdown_request_in_progress = in_progress;
}

InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event)
{
    InputResult result = {};
    bool request_refresh = false;
    bool play_click = false;
    bool dismiss_modal = false;

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return result;
        }
        if (s_shutdown_request_in_progress) {
            result.consumed = true;
            return result;
        }
        if (!s_shutdown_modal_state.visible) {
            return result;
        }

        result.consumed = true;
        switch (event.event) {
            case button_service::ButtonEvent::kSingleClick:
                if (event.button == button_service::ButtonId::kUp &&
                    s_shutdown_modal_state.selected_action !=
                        epaper_ui::ShutdownModalAction::kCancel) {
                    s_shutdown_modal_state.selected_action =
                        epaper_ui::ShutdownModalAction::kCancel;
                    request_refresh = true;
                    play_click = true;
                } else if (event.button == button_service::ButtonId::kDown &&
                           s_shutdown_modal_state.selected_action !=
                               epaper_ui::ShutdownModalAction::kConfirm) {
                    s_shutdown_modal_state.selected_action =
                        epaper_ui::ShutdownModalAction::kConfirm;
                    request_refresh = true;
                    play_click = true;
                }
                break;
            case button_service::ButtonEvent::kDoubleClick:
                if (event.button == button_service::ButtonId::kDown) {
                    if (s_shutdown_modal_state.selected_action ==
                        epaper_ui::ShutdownModalAction::kConfirm) {
                        s_shutdown_request_in_progress = true;
                        result.request_shutdown = true;
                    } else {
                        play_click = true;
                    }
                    s_shutdown_modal_state.visible = false;
                    s_shutdown_modal_state.selected_action =
                        epaper_ui::ShutdownModalAction::kCancel;
                    request_refresh = true;
                    dismiss_modal = true;
                }
                break;
            case button_service::ButtonEvent::kPressDown:
            case button_service::ButtonEvent::kPressUp:
            case button_service::ButtonEvent::kLongPressStart:
            case button_service::ButtonEvent::kLongPressUp:
            default:
                break;
        }
    }

    if (play_click) {
        (void)feedback_service::Play(feedback_service::FeedbackEvent::kButtonClick);
    }
    if (dismiss_modal) {
        ESP_LOGI(kTag,
                 "Shutdown modal button action=%s",
                 result.request_shutdown ? "confirm" : "cancel");
    }
    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after button failed: %s", esp_err_to_name(err));
        }
    }
    return result;
}

InputResult HandleTouchEvent(const touch_service::TouchEventInfo& event)
{
    InputResult result = {};
    bool request_refresh = false;
    bool play_touch_feedback = false;
    bool action_cooldown_active = false;
    uint64_t now_ms = MonotonicMs();

    int portrait_width = 0;
    int portrait_height = 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return result;
        }
        if (s_shutdown_request_in_progress) {
            result.consumed = true;
            return result;
        }
        if (event.count > 0 && now_ms >= s_touch_feedback_cooldown_until_ms) {
            s_touch_feedback_cooldown_until_ms = now_ms + kTouchFeedbackCooldownMs;
            play_touch_feedback = true;
        }
        if (now_ms < s_touch_action_cooldown_until_ms) {
            result.consumed = true;
            action_cooldown_active = true;
        }
        if (action_cooldown_active) {
            // Keep consuming samples briefly after a modal touch so one press
            // doesn't re-trigger as the controller continues to report contact.
            if (play_touch_feedback) {
                (void)feedback_service::Play(feedback_service::FeedbackEvent::kTouchContact);
            }
            return result;
        }
        if (!s_shutdown_modal_state.visible || event.count == 0) {
            return result;
        }

        result.consumed = true;
    }

    portrait_width = display_service::PortraitWidth();
    portrait_height = display_service::PortraitHeight();

    epaper_ui::ShutdownModalAction action = epaper_ui::ShutdownModalAction::kCancel;
    const int touch_x = static_cast<int>(event.points[0].x);
    const int touch_y = static_cast<int>(event.points[0].y);
    if (!HitShutdownActionAnyOrientation(portrait_width,
                                         portrait_height,
                                         touch_x,
                                         touch_y,
                                         &action)) {
        ESP_LOGI(kTag, "Shutdown modal touch miss: x=%d y=%d", touch_x, touch_y);
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_shutdown_modal_state.visible) {
            return result;
        }
        s_shutdown_modal_state.selected_action = action;
        s_shutdown_modal_state.visible = false;
        s_touch_action_cooldown_until_ms = now_ms + kTouchActionCooldownMs;
        if (action == epaper_ui::ShutdownModalAction::kConfirm) {
            s_shutdown_request_in_progress = true;
            result.request_shutdown = true;
        }
        request_refresh = true;
    }

    if (play_touch_feedback) {
        (void)feedback_service::Play(feedback_service::FeedbackEvent::kTouchContact);
    }
    if (request_refresh) {
        ESP_LOGI(kTag,
                 "Shutdown modal touch action=%s x=%d y=%d",
                 result.request_shutdown ? "confirm" : "cancel",
                 touch_x,
                 touch_y);
    }
    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after touch failed: %s", esp_err_to_name(err));
        }
    }
    return result;
}

esp_err_t ShowToast(const epaper_ui::ToastState& state)
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        s_toast_state = state;
    }
    return SyncOverlayState(true);
}

esp_err_t ClearToast()
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        s_toast_state = {};
    }
    return SyncOverlayState(true);
}

}  // namespace overlay_runtime
