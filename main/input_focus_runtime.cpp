#include "input_focus_runtime.h"

#include "overlay_runtime.h"
#include "page_input_runtime.h"

namespace input_focus_runtime {
namespace {

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

}  // namespace input_focus_runtime
