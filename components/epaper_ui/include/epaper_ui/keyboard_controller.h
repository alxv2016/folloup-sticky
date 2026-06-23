#ifndef EPAPER_UI_KEYBOARD_CONTROLLER_H_
#define EPAPER_UI_KEYBOARD_CONTROLLER_H_

#include "epaper_ui/keyboard.h"

namespace epaper_ui {

enum class KeyboardIntent : uint8_t {
    kNone = 0,
    kSubmit,
    kDismiss,
};

struct KeyboardActionResult {
    bool text_changed = false;
    bool state_changed = false;
    KeyboardIntent intent = KeyboardIntent::kNone;
};

class KeyboardController {
public:
    static bool MoveFocus(KeyboardState& state, int delta);
    static KeyboardActionResult ActivateFocusedKey(KeyboardState& state, bool long_press);
    static KeyboardActionResult ActivateFocusedKeyDouble(KeyboardState& state);
    static void ClampSelection(KeyboardState& state);
};

}  // namespace epaper_ui

#endif  // EPAPER_UI_KEYBOARD_CONTROLLER_H_
