#include "epaper_ui/password_input.h"

#include "epaper_ui/text_input.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

// PasswordInput is a TextInput with masking driven by password_visible and a trailing
// visibility-toggle icon. Map the password-specific state/style onto the shared primitive.
TextInputState ToTextInput(const PasswordInputState& state)
{
    return {
        .label_text = state.label_text,
        .placeholder_text = state.placeholder_text,
        .value_text = state.value_text,
        .focused = state.focused,
        .active = state.active,
        .mask = !state.password_visible,
        .show_cursor = state.show_cursor,
        .cursor_index = state.cursor_index,
        .max_length = state.max_length,
        .submit_style = state.submit_style,
        .has_trailing_icon = true,
        .trailing_icon = state.password_visible ? EmbeddedIconId::kVisible
                                                : EmbeddedIconId::kInvisible,
        .trailing_focused = state.visibility_button_focused,
    };
}

TextInputStyle ToTextStyle(const PasswordInputStyle& style)
{
    return {
        .width = style.width,
        .field_height = style.field_height,
        .horizontal_padding = style.horizontal_padding,
        .icon_size = style.icon_size,
        .icon_padding = style.icon_padding,
        .label_gap = style.label_gap,
        .border_thickness = style.border_thickness,
        .focus_ring_thickness = style.focus_ring_thickness,
        .focus_gap = style.focus_gap,
        .background_color = style.background_color,
        .border_color = style.border_color,
        .text_color = style.text_color,
        .placeholder_color = style.placeholder_color,
        .icon_color = style.icon_color,
        .focus_ring_color = style.focus_ring_color,
        .focus_gap_color = style.focus_gap_color,
        .label_role = style.label_role,
        .value_role = style.value_role,
        .placeholder_role = style.placeholder_role,
    };
}

}  // namespace

UiRect PasswordInputBounds(int origin_x,
                           int origin_y,
                           const PasswordInputState& state,
                           const PasswordInputStyle& style)
{
    return TextInputBounds(origin_x, origin_y, ToTextInput(state), ToTextStyle(style));
}

UiRect PasswordInputFieldBounds(int origin_x,
                                int origin_y,
                                const PasswordInputState& state,
                                const PasswordInputStyle& style)
{
    return TextInputFieldBounds(origin_x, origin_y, ToTextInput(state), ToTextStyle(style));
}

UiRect PasswordInputVisualBounds(int origin_x,
                                 int origin_y,
                                 const PasswordInputState& state,
                                 const PasswordInputStyle& style)
{
    return TextInputVisualBounds(origin_x, origin_y, ToTextInput(state), ToTextStyle(style));
}

UiRect PasswordInputVisibilityButtonBounds(int origin_x,
                                           int origin_y,
                                           const PasswordInputState& state,
                                           const PasswordInputStyle& style)
{
    return TextInputTrailingBounds(origin_x, origin_y, ToTextInput(state), ToTextStyle(style));
}

KeyboardInputState PasswordInputToKeyboardInput(const PasswordInputState& state)
{
    return TextInputToKeyboardInput(ToTextInput(state));
}

void ApplyKeyboardInputToPasswordInput(PasswordInputState* state,
                                       const KeyboardInputState& input_state)
{
    if (state == nullptr) {
        return;
    }
    state->label_text = input_state.label_text;
    state->placeholder_text = input_state.placeholder_text;
    state->value_text = input_state.value_text;
    state->focused = input_state.focused;
    state->active = input_state.active;
    state->password_visible = !input_state.password_mode;
    state->show_cursor = input_state.show_cursor;
    state->cursor_index = input_state.cursor_index;
    state->max_length = input_state.max_length;
    state->submit_style = input_state.submit_style;
}

void DrawPasswordInput(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int origin_x,
                       int origin_y,
                       const PasswordInputState& state,
                       const PasswordInputStyle& style)
{
    DrawTextInput(framebuffer,
                  raw_width,
                  raw_height,
                  portrait_width,
                  portrait_height,
                  origin_x,
                  origin_y,
                  ToTextInput(state),
                  ToTextStyle(style));
}

}  // namespace epaper_ui
