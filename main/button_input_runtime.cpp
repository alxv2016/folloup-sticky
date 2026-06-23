#include "button_input_runtime.h"

#include <cstddef>
#include <utility>

#include "input_callback_dispatcher.h"
#include "page_navigation/navigation_input_controller.h"

namespace button_input_runtime {
namespace {

constexpr uint32_t kDispatchKeyNavigationUpPress = 1;
constexpr uint32_t kDispatchKeyNavigationDownPress = 2;
constexpr size_t kHoldSlotUp = 0;
constexpr size_t kHoldSlotDown = 1;

page_navigation::NavigationInputController s_navigation_input;

bool IsNavigationButton(button_service::ButtonId button) {
    return button == button_service::ButtonId::kUp ||
           button == button_service::ButtonId::kDown;
}

size_t HoldSlotForButton(button_service::ButtonId button) {
    return button == button_service::ButtonId::kUp ? kHoldSlotUp : kHoldSlotDown;
}

uint32_t DispatchKeyForButton(button_service::ButtonId button) {
    return button == button_service::ButtonId::kUp ? kDispatchKeyNavigationUpPress
                                                   : kDispatchKeyNavigationDownPress;
}

}  // namespace

esp_err_t Init() {
    InputCallbackDispatcher::GetInstance().Initialize();
    return ESP_OK;
}

void HandleHardwareEvent(const button_service::ButtonEventInfo& event,
                         DispatchHandler dispatch_handler) {
    if (!dispatch_handler) {
        return;
    }

    if (!IsNavigationButton(event.button)) {
        InputCallbackDispatcher::GetInstance().Dispatch(
            [event, dispatch_handler = std::move(dispatch_handler)]() {
                dispatch_handler(event);
            });
        return;
    }

    const size_t hold_slot = HoldSlotForButton(event.button);
    switch (event.event) {
        case button_service::ButtonEvent::kPressDown:
            s_navigation_input.BeginPress(hold_slot);
            InputCallbackDispatcher::GetInstance().DispatchLatest(
                DispatchKeyForButton(event.button),
                [event, dispatch_handler = std::move(dispatch_handler)]() {
                    dispatch_handler(event);
                });
            return;
        case button_service::ButtonEvent::kPressUp:
            s_navigation_input.EndPress(hold_slot);
            InputCallbackDispatcher::GetInstance().Dispatch(
                [event, dispatch_handler = std::move(dispatch_handler)]() {
                    dispatch_handler(event);
                });
            return;
        case button_service::ButtonEvent::kPressRepeat: {
            const uint32_t generation = s_navigation_input.CurrentGeneration(hold_slot);
            InputCallbackDispatcher::GetInstance().Dispatch(
                [event,
                 hold_slot,
                 generation,
                 dispatch_handler = std::move(dispatch_handler)]() {
                    if (!s_navigation_input.ShouldHandleHold(hold_slot, generation,
                                                             event.pressed_ms)) {
                        return;
                    }
                    dispatch_handler(event);
                });
            return;
        }
        case button_service::ButtonEvent::kSingleClick:
        case button_service::ButtonEvent::kDoubleClick:
        case button_service::ButtonEvent::kLongPressStart:
        case button_service::ButtonEvent::kLongPressUp:
        default:
            InputCallbackDispatcher::GetInstance().Dispatch(
                [event, dispatch_handler = std::move(dispatch_handler)]() {
                    dispatch_handler(event);
                });
            return;
    }
}

}  // namespace button_input_runtime
