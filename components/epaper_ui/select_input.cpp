#include "epaper_ui/select_input.h"

namespace epaper_ui {
namespace {

TextInputState ToTextInput(const SelectInputState& state)
{
    TextInputState text = {};
    text.label_text = state.label_text;
    text.placeholder_text = state.placeholder_text;
    text.value_text = state.value_text;
    text.focused = state.focused;
    text.active = false;       // read-only: no editing cursor
    text.show_cursor = false;
    text.mask = false;
    text.has_trailing_icon = true;
    text.trailing_icon = EmbeddedIconId::kSelect;
    text.trailing_focused = false;
    return text;
}

}  // namespace

UiRect SelectInputBounds(int origin_x,
                         int origin_y,
                         const SelectInputState& state,
                         const TextInputStyle& style)
{
    return TextInputBounds(origin_x, origin_y, ToTextInput(state), style);
}

UiRect SelectInputVisualBounds(int origin_x,
                               int origin_y,
                               const SelectInputState& state,
                               const TextInputStyle& style)
{
    return TextInputVisualBounds(origin_x, origin_y, ToTextInput(state), style);
}

void DrawSelectInput(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const SelectInputState& state,
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
