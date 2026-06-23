#include "epaper_ui/password_input.h"

#include <algorithm>

#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

int FocusPadding(const PasswordInputStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

std::string BuildDisplayText(const PasswordInputState& state)
{
    std::string display = state.password_visible ? state.value_text
                                                 : std::string(state.value_text.size(), '*');
    if (!state.active || !state.show_cursor) {
        return display;
    }

    const int cursor =
        std::clamp(state.cursor_index < 0 ? static_cast<int>(display.size()) : state.cursor_index,
                   0,
                   static_cast<int>(display.size()));
    display.insert(display.begin() + cursor, '|');
    return display;
}

std::string FitText(std::string_view text, design::TypographyRole role, int max_width)
{
    if (text.empty() || max_width <= 0) {
        return {};
    }
    if (MeasureText(role, text) <= max_width) {
        return std::string(text);
    }

    size_t length = text.size();
    while (length > 0) {
        std::string_view candidate = text.substr(0, length);
        if (MeasureText(role, candidate) <= max_width) {
            return std::string(candidate);
        }
        --length;
    }
    return {};
}

UiRect Inset(const UiRect& rect, int inset)
{
    return {rect.x + inset,
            rect.y + inset,
            std::max(0, rect.width - (2 * inset)),
            std::max(0, rect.height - (2 * inset))};
}

UiRect Expand(const UiRect& rect, int inset)
{
    return {rect.x - inset,
            rect.y - inset,
            rect.width + (2 * inset),
            rect.height + (2 * inset)};
}

}  // namespace

UiRect PasswordInputBounds(int origin_x,
                           int origin_y,
                           const PasswordInputState& state,
                           const PasswordInputStyle& style)
{
    const int width = std::max(0, style.width);
    const int label_height = state.label_text.empty() ? 0 : LineHeight(style.label_role);
    const int label_gap = label_height > 0 ? ClampPositive(style.label_gap) : 0;
    const int field_height = ClampPositive(style.field_height);
    return {origin_x, origin_y, width, label_height + label_gap + field_height};
}

UiRect PasswordInputFieldBounds(int origin_x,
                                int origin_y,
                                const PasswordInputState& state,
                                const PasswordInputStyle& style)
{
    const UiRect bounds = PasswordInputBounds(origin_x, origin_y, state, style);
    const int label_height = state.label_text.empty() ? 0 : LineHeight(style.label_role);
    const int label_gap = label_height > 0 ? ClampPositive(style.label_gap) : 0;
    return {bounds.x,
            bounds.y + label_height + label_gap,
            bounds.width,
            ClampPositive(style.field_height)};
}

UiRect PasswordInputVisibilityButtonBounds(int origin_x,
                                           int origin_y,
                                           const PasswordInputState& state,
                                           const PasswordInputStyle& style)
{
    const UiRect field = PasswordInputFieldBounds(origin_x, origin_y, state, style);
    const UiRect inner = Inset(field, ClampPositive(style.border_thickness));
    const EmbeddedImageAsset* asset =
        project_assets::GetIcon(state.password_visible ? EmbeddedIconId::kVisible
                                                       : EmbeddedIconId::kInvisible);
    const int slot_size = ClampPositive(style.icon_size);
    const int slot_x = inner.right() - ClampPositive(style.icon_padding) - slot_size;
    const int slot_y = inner.y + CenterOffset(inner.height, slot_size);
    (void)asset;
    return {slot_x, slot_y, slot_size, slot_size};
}

KeyboardInputState PasswordInputToKeyboardInput(const PasswordInputState& state)
{
    return {
        .label_text = state.label_text,
        .placeholder_text = state.placeholder_text,
        .value_text = state.value_text,
        .focused = state.focused,
        .active = state.active,
        .password_mode = !state.password_visible,
        .show_cursor = state.show_cursor,
        .cursor_index = state.cursor_index,
        .max_length = state.max_length,
        .submit_style = state.submit_style,
    };
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
    const UiRect bounds = PasswordInputBounds(origin_x, origin_y, state, style);
    if (bounds.IsEmpty()) {
        return;
    }

    if (!state.label_text.empty()) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           bounds.x,
                           bounds.y,
                           state.label_text,
                           style.label_role,
                           style.text_color);
    }

    const UiRect field = PasswordInputFieldBounds(origin_x, origin_y, state, style);
    if (state.focused) {
        const UiRect ring = Expand(field, FocusPadding(style));
        const UiRect gap = Expand(field, ClampPositive(style.focus_gap));
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                ring,
                                kControlCornerRadius + FocusPadding(style),
                                style.focus_ring_color);
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                gap,
                                kControlCornerRadius + ClampPositive(style.focus_gap),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            field,
                            kControlCornerRadius,
                            style.background_color);
    DrawRoundedPortraitBorder(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              field,
                              kControlCornerRadius,
                              style.border_thickness,
                              style.border_color);

    const UiRect inner = Inset(field, ClampPositive(style.border_thickness));
    const UiRect visibility = PasswordInputVisibilityButtonBounds(origin_x, origin_y, state, style);
    if (state.visibility_button_focused && !visibility.IsEmpty()) {
        const int padding = std::max(2, ClampPositive(style.icon_padding) / 2);
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                Expand(visibility, padding),
                                kControlCornerRadius,
                                style.icon_color);
    }

    const EmbeddedImageAsset* visibility_asset =
        project_assets::GetIcon(state.password_visible ? EmbeddedIconId::kVisible
                                                       : EmbeddedIconId::kInvisible);
    if (visibility_asset != nullptr && !visibility.IsEmpty()) {
        const int icon_x = visibility.x + CenterOffset(visibility.width, visibility_asset->width);
        const int icon_y =
            visibility.y + CenterOffset(visibility.height, visibility_asset->height);
        DrawPortraitMonoAsset(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              icon_x,
                              icon_y,
                              visibility_asset,
                              state.visibility_button_focused ? style.background_color
                                                              : style.icon_color);
    }

    const bool show_placeholder = state.value_text.empty() && !state.placeholder_text.empty();
    const std::string raw_text =
        show_placeholder ? state.placeholder_text : BuildDisplayText(state);
    const design::TypographyRole role =
        show_placeholder ? style.placeholder_role : style.value_role;
    const uint8_t tone = show_placeholder ? style.placeholder_color : style.text_color;
    const int right_inset =
        visibility.width > 0
            ? std::max(0, inner.right() - visibility.x) + ClampPositive(style.horizontal_padding)
            : ClampPositive(style.horizontal_padding);
    const int max_width =
        std::max(0, inner.width - ClampPositive(style.horizontal_padding) - right_inset);
    const std::string text = FitText(raw_text, role, max_width);
    if (text.empty()) {
        return;
    }

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       inner.x + ClampPositive(style.horizontal_padding),
                       inner.y + CenterOffset(inner.height, LineHeight(role)),
                       text,
                       role,
                       tone);
}

}  // namespace epaper_ui
