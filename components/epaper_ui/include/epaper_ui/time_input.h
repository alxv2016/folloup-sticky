#ifndef EPAPER_UI_TIME_INPUT_H_
#define EPAPER_UI_TIME_INPUT_H_

#include <cstddef>
#include <string>

#include "epaper_ui/keyboard_input.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/text_input.h"

namespace epaper_ui {

// A compact numeric field with an optional right-aligned suffix (e.g. "HR", "MIN").
// Activating it opens the numeric keyboard (owned by the caller). Built on TextInput.
struct TimeInputState {
    std::string value_text = {};
    std::string placeholder_text = {};
    std::string suffix_text = {};
    bool focused = false;
    bool active = false;
    bool show_cursor = true;
    int cursor_index = -1;
    size_t max_length = 4;

    bool operator==(const TimeInputState& other) const = default;
};

UiRect TimeInputBounds(int origin_x,
                       int origin_y,
                       const TimeInputState& state,
                       const TextInputStyle& style);
UiRect TimeInputVisualBounds(int origin_x,
                             int origin_y,
                             const TimeInputState& state,
                             const TextInputStyle& style);
KeyboardInputState TimeInputToKeyboardInput(const TimeInputState& state);
void ApplyKeyboardInputToTimeInput(TimeInputState* state, const KeyboardInputState& input_state);
void DrawTimeInput(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   int origin_x,
                   int origin_y,
                   const TimeInputState& state,
                   const TextInputStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TIME_INPUT_H_
