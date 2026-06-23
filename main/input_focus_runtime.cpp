#include "input_focus_runtime.h"

#include <mutex>

#include "display_service.h"
#include "esp_log.h"
#include "footer_runtime.h"
#include "page_input_runtime.h"
#include "page_interaction_runtime.h"

namespace input_focus_runtime {
namespace {

constexpr const char* kTag = "InputFocusRuntime";

struct TouchInteractionState {
    bool contact_active = false;
    app_interaction::InteractiveTarget armed_target = {};
    app_interaction::InteractiveTarget focused_target = {};
};

std::mutex s_touch_state_mutex;
TouchInteractionState s_touch_state = {};

bool IsNavigationMoveEvent(const button_service::ButtonEventInfo& event, int* delta)
{
    if (delta != nullptr) {
        *delta = 0;
    }

    if (event.event != button_service::ButtonEvent::kPressDown &&
        event.event != button_service::ButtonEvent::kPressRepeat) {
        return false;
    }

    if (event.button == button_service::ButtonId::kUp) {
        if (delta != nullptr) {
            *delta = -1;
        }
        return true;
    }

    if (event.button == button_service::ButtonId::kDown) {
        if (delta != nullptr) {
            *delta = 1;
        }
        return true;
    }

    return false;
}

void ResetTouchStateLocked()
{
    s_touch_state.contact_active = false;
    s_touch_state.armed_target = {};
    s_touch_state.focused_target = {};
}

bool ResolveTouchTarget(const touch_service::TouchEventInfo& event,
                        app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    if (event.count == 0) {
        return false;
    }

    if (overlay_runtime::IsInputCaptured()) {
        const bool hit = overlay_runtime::ResolveTouchTarget(
            static_cast<int>(event.points[0].x), static_cast<int>(event.points[0].y), target);
        ESP_LOGD(kTag, "Touch precedence: owner=overlay hit=%d", hit ? 1 : 0);
        return hit;
    }

    if (footer_runtime::ResolveTouchTarget(
            static_cast<int>(event.points[0].x), static_cast<int>(event.points[0].y), target)) {
        ESP_LOGD(kTag, "Touch precedence: owner=footer hit=1");
        return true;
    }

    if (page_interaction_runtime::ResolveTouchTarget(
            static_cast<int>(event.points[0].x), static_cast<int>(event.points[0].y), target)) {
        ESP_LOGD(kTag, "Touch precedence: owner=page hit=1");
        return true;
    }

    ESP_LOGD(kTag, "Touch precedence: no interactive target hit");
    return false;
}

}  // namespace

app_interaction::InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event)
{
    app_interaction::InputResult result = {};
    int delta = 0;
    if (IsNavigationMoveEvent(event, &delta) && overlay_runtime::IsInputCaptured()) {
        result.consumed = true;
        if (overlay_runtime::MoveFocus(delta)) {
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
        }
        return result;
    }

    if (IsNavigationMoveEvent(event, &delta)) {
        const page_input_runtime::FocusMoveResult move_result =
            page_input_runtime::MoveFocusForCurrentScreen(
                delta, event.event == button_service::ButtonEvent::kPressRepeat);
        if (move_result.handled) {
            return move_result.interaction_result;
        }
    }

    return overlay_runtime::HandleButtonEvent(event);
}

app_interaction::InputResult HandleTouchEvent(const touch_service::TouchEventInfo& event)
{
    app_interaction::InputResult result = {};
    app_interaction::InteractiveTarget resolved_target = {};
    const bool target_hit = ResolveTouchTarget(event, &resolved_target);
    bool has_prior_touch_context = false;
    {
        std::lock_guard<std::mutex> lock(s_touch_state_mutex);
        has_prior_touch_context =
            s_touch_state.contact_active || s_touch_state.armed_target.IsValid() ||
            s_touch_state.focused_target.IsValid();
    }

    if (!target_hit && !has_prior_touch_context) {
        if (event.phase == touch_service::TouchPhase::kBegin) {
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kTouchContact;
        }
        if (event.count > 0) {
            ESP_LOGI(kTag,
                     "Touch miss phase=%d x=%u y=%u size=%u id=%u",
                     static_cast<int>(event.phase),
                     static_cast<unsigned>(event.points[0].x),
                     static_cast<unsigned>(event.points[0].y),
                     static_cast<unsigned>(event.points[0].size),
                     static_cast<unsigned>(event.points[0].id));
        }
        return result;
    }

    switch (event.phase) {
        case touch_service::TouchPhase::kBegin:
        case touch_service::TouchPhase::kMove: {
            bool play_touch_feedback = false;
            result.consumed = target_hit || has_prior_touch_context;
            {
                std::lock_guard<std::mutex> lock(s_touch_state_mutex);
                s_touch_state.contact_active = true;
                if (target_hit) {
                    ESP_LOGI(kTag,
                             "Touch target resolved owner=%d kind=%d primary=%ld",
                             static_cast<int>(resolved_target.owner),
                             static_cast<int>(resolved_target.kind),
                             static_cast<long>(resolved_target.primary_index));
                    bool focus_changed = false;
                    if (resolved_target.owner == app_interaction::Owner::kOverlay) {
                        focus_changed = overlay_runtime::FocusTouchTarget(resolved_target);
                    } else if (resolved_target.owner == app_interaction::Owner::kFooter) {
                        focus_changed =
                            page_input_runtime::FocusFooterTouchTargetForCurrentScreen(
                                resolved_target) ||
                            footer_runtime::FocusTouchTarget(resolved_target);
                    } else if (resolved_target.owner == app_interaction::Owner::kPage) {
                        focus_changed = page_interaction_runtime::FocusTouchTarget(resolved_target);
                    }
                    if (resolved_target != s_touch_state.focused_target && focus_changed) {
                        play_touch_feedback = true;
                    }
                    s_touch_state.focused_target = resolved_target;
                    s_touch_state.armed_target = resolved_target;
                    ESP_LOGI(kTag,
                             "Touch focus target owner=%d kind=%d primary=%ld",
                             static_cast<int>(resolved_target.owner),
                             static_cast<int>(resolved_target.kind),
                             static_cast<long>(resolved_target.primary_index));
                } else {
                    s_touch_state.armed_target = {};
                    s_touch_state.focused_target = {};
                    ESP_LOGI(kTag, "Touch focus cleared: no interactive target hit");
                }
            }
            if (play_touch_feedback) {
                result.play_feedback = true;
                result.feedback_cue = app_interaction::FeedbackCue::kTouchContact;
            }
            return result;
        }
        case touch_service::TouchPhase::kEnd: {
            app_interaction::InteractiveTarget activated_target = {};
            {
                std::lock_guard<std::mutex> lock(s_touch_state_mutex);
                activated_target = s_touch_state.armed_target;
                ResetTouchStateLocked();
            }

            if (!activated_target.IsValid()) {
                ESP_LOGI(kTag, "Touch activation canceled: no armed target on release");
                result.consumed = has_prior_touch_context;
                return result;
            }

            ESP_LOGI(kTag,
                     "Touch activate target owner=%d kind=%d primary=%ld",
                     static_cast<int>(activated_target.owner),
                     static_cast<int>(activated_target.kind),
                     static_cast<long>(activated_target.primary_index));
            app_interaction::InputResult activate_result = {};
            if (activated_target.owner == app_interaction::Owner::kOverlay) {
                activate_result = overlay_runtime::ActivateTouchTarget(activated_target);
            } else if (activated_target.owner == app_interaction::Owner::kFooter) {
                activate_result = footer_runtime::ActivateTouchTarget(activated_target);
            } else if (activated_target.owner == app_interaction::Owner::kPage) {
                activate_result = page_interaction_runtime::ActivateTouchTarget(activated_target);
            }
            activate_result.consumed = true;
            return activate_result;
        }
        case touch_service::TouchPhase::kNone:
        default:
            return result;
    }
}

}  // namespace input_focus_runtime
