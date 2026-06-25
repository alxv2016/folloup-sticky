#include "epaper_ui/time_input.h"

namespace epaper_ui {
namespace {

TextInputState ToTextInput(const TimeInputState& state)
{
    TextInputState text = {};
    text.placeholder_text = state.placeholder_text;
    text.value_text = state.value_text;
    text.suffix_text = state.suffix_text;
    text.focused = state.focused;
    text.active = state.active;
    text.show_cursor = state.show_cursor;
    text.cursor_index = state.cursor_index;
    text.max_length = state.max_length;
    text.mask = false;
    text.has_trailing_icon = false;
    return text;
}

}  // namespace

UiRect TimeInputBounds(int origin_x,
                       int origin_y,
                       const TimeInputState& state,
                       const TextInputStyle& style)
{
    return TextInputBounds(origin_x, origin_y, ToTextInput(state), style);
}

UiRect TimeInputVisualBounds(int origin_x,
                             int origin_y,
                             const TimeInputState& state,
                             const TextInputStyle& style)
{
    return TextInputVisualBounds(origin_x, origin_y, ToTextInput(state), style);
}

KeyboardInputState TimeInputToKeyboardInput(const TimeInputState& state)
{
    return TextInputToKeyboardInput(ToTextInput(state));
}

void ApplyKeyboardInputToTimeInput(TimeInputState* state, const KeyboardInputState& input_state)
{
    if (state == nullptr) {
        return;
    }
    state->value_text = input_state.value_text;
    state->placeholder_text = input_state.placeholder_text;
    state->focused = input_state.focused;
    state->active = input_state.active;
    state->show_cursor = input_state.show_cursor;
    state->cursor_index = input_state.cursor_index;
    state->max_length = input_state.max_length;
}

void DrawTimeInput(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   int origin_x,
                   int origin_y,
                   const TimeInputState& state,
                   const TextInputStyle& style)
{
    DrawTextInput(framebuffer,
                  raw_width,
                  raw_height,
                  portrait_width,
                  portrait_height,
                  origin_x,
                  origin_y,
                  ToTextInput(state),
                  style);
}

}  // namespace epaper_ui
