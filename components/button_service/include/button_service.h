#ifndef BUTTON_SERVICE_H_
#define BUTTON_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace button_service {

enum class ButtonId {
    kPowerOk,
    kUp,
    kDown,
};

enum class ButtonEvent {
    kPressDown,
    kPressUp,
    kSingleClick,
    kDoubleClick,
    kLongPressStart,
    kLongPressUp,
};

struct ButtonEventInfo {
    ButtonId button = ButtonId::kPowerOk;
    ButtonEvent event = ButtonEvent::kPressDown;
    uint32_t pressed_ms = 0;
};

using EventHandler = void (*)(const ButtonEventInfo& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);

}  // namespace button_service

#endif  // BUTTON_SERVICE_H_
