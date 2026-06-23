#ifndef EPAPER_UI_KEYBOARD_INPUT_H_
#define EPAPER_UI_KEYBOARD_INPUT_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace epaper_ui {

enum class KeyboardInputSubmitStyle : uint8_t {
    kDone = 0,
    kJoin,
    kSave,
    kNext,
};

struct KeyboardInputState {
    std::string label_text = {};
    std::string placeholder_text = {};
    std::string value_text = {};
    bool focused = false;
    bool active = false;
    bool password_mode = false;
    bool show_cursor = true;
    int cursor_index = -1;
    size_t max_length = 64;
    KeyboardInputSubmitStyle submit_style = KeyboardInputSubmitStyle::kDone;

    bool operator==(const KeyboardInputState& other) const = default;
};

}  // namespace epaper_ui

#endif  // EPAPER_UI_KEYBOARD_INPUT_H_
