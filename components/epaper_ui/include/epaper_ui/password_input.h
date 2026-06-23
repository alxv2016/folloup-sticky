#ifndef EPAPER_UI_PASSWORD_INPUT_H_
#define EPAPER_UI_PASSWORD_INPUT_H_

#include <cstddef>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/keyboard_input.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct PasswordInputState {
    std::string label_text = {};
    std::string placeholder_text = {};
    std::string value_text = {};
    bool focused = false;
    bool visibility_button_focused = false;
    bool active = false;
    bool password_visible = false;
    bool show_cursor = true;
    int cursor_index = -1;
    size_t max_length = 64;
    KeyboardInputSubmitStyle submit_style = KeyboardInputSubmitStyle::kDone;

    bool operator==(const PasswordInputState& other) const = default;
};

struct PasswordInputStyle {
    int width = 0;
    int field_height = design::password_input::kFieldHeight;
    int horizontal_padding = design::password_input::kHorizontalPadding;
    int icon_size = design::password_input::kIconSize;
    int icon_padding = design::password_input::kIconPadding;
    int label_gap = design::password_input::kLabelGap;
    int border_thickness = design::password_input::kBorderThickness;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
    uint8_t background_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t placeholder_color = design::color::kGrayDark;
    uint8_t icon_color = design::color::kBlack;
    uint8_t focus_ring_color = design::color::kBlack;
    uint8_t focus_gap_color = design::color::kWhite;
    design::TypographyRole label_role = design::TypographyRole::kLabelMedium;
    design::TypographyRole value_role = design::TypographyRole::kInputValue;
    design::TypographyRole placeholder_role = design::TypographyRole::kInputValue;
};

UiRect PasswordInputBounds(int origin_x,
                           int origin_y,
                           const PasswordInputState& state,
                           const PasswordInputStyle& style);
UiRect PasswordInputFieldBounds(int origin_x,
                                int origin_y,
                                const PasswordInputState& state,
                                const PasswordInputStyle& style);
UiRect PasswordInputVisualBounds(int origin_x,
                                 int origin_y,
                                 const PasswordInputState& state,
                                 const PasswordInputStyle& style);
UiRect PasswordInputVisibilityButtonBounds(int origin_x,
                                           int origin_y,
                                           const PasswordInputState& state,
                                           const PasswordInputStyle& style);
KeyboardInputState PasswordInputToKeyboardInput(const PasswordInputState& state);
void ApplyKeyboardInputToPasswordInput(PasswordInputState* state,
                                       const KeyboardInputState& input_state);
void DrawPasswordInput(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int origin_x,
                       int origin_y,
                       const PasswordInputState& state,
                       const PasswordInputStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_PASSWORD_INPUT_H_
