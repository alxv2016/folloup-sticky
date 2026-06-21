#include "input_focus_runtime.h"

#include "feedback_service.h"

namespace input_focus_runtime {
namespace {

bool IsNavigationMoveEvent(const button_service::ButtonEventInfo& event, int* delta)
{
    if (delta != nullptr) {
        *delta = 0;
    }

    if (event.event != button_service::ButtonEvent::kSingleClick) {
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

overlay_runtime::InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event)
{
    int delta = 0;
    if (IsNavigationMoveEvent(event, &delta) && overlay_runtime::IsInputCaptured()) {
        overlay_runtime::InputResult result = {};
        result.consumed = true;
        if (overlay_runtime::MoveFocus(delta)) {
            (void)feedback_service::Play(feedback_service::FeedbackEvent::kButtonClick);
        }
        return result;
    }

    return overlay_runtime::HandleButtonEvent(event);
}

}  // namespace input_focus_runtime
