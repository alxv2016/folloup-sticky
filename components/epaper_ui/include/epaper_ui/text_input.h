#ifndef EPAPER_UI_TEXT_INPUT_H_
#define EPAPER_UI_TEXT_INPUT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/keyboard_input.h"
#include "epaper_ui/overlay_geometry.h"
#include "project_assets.h"

namespace epaper_ui {

// A labeled single-line text field: optional label on top, a rounded field showing the
// value (or placeholder), an optional editing cursor, an optional focus ring, optional
// masking (password style), and an optional trailing icon button (e.g. password
// visibility). PasswordInput and SelectInput are built on top of this primitive.
struct TextInputState {
    std::string label_text = {};
    std::string placeholder_text = {};
    std::string value_text = {};
    bool focused = false;
    bool active = false;  // editing: show the cursor in the value
    bool mask = false;    // render the value as '*' (password style)
    bool show_cursor = true;
    int cursor_index = -1;
    size_t max_length = 64;
    KeyboardInputSubmitStyle submit_style = KeyboardInputSubmitStyle::kDone;

    // Optional short suffix drawn right-aligned inside the field (e.g. "HR", "MIN" for
    // TimeInput). Mutually exclusive with the trailing icon in practice.
    std::string suffix_text = {};

    // Optional trailing affordance drawn inside the field on the right (e.g. a password
    // visibility toggle or a dropdown chevron). The owner reacts to taps on its bounds.
    bool has_trailing_icon = false;
    EmbeddedIconId trailing_icon = EmbeddedIconId::kVisible;
    bool trailing_focused = false;

    bool operator==(const TextInputState& other) const = default;
};

struct TextInputStyle {
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

UiRect TextInputBounds(int origin_x,
                       int origin_y,
                       const TextInputState& state,
                       const TextInputStyle& style);
UiRect TextInputFieldBounds(int origin_x,
                            int origin_y,
                            const TextInputState& state,
                            const TextInputStyle& style);
UiRect TextInputVisualBounds(int origin_x,
                             int origin_y,
                             const TextInputState& state,
                             const TextInputStyle& style);
// Bounds of the trailing icon button; empty when has_trailing_icon is false.
UiRect TextInputTrailingBounds(int origin_x,
                               int origin_y,
                               const TextInputState& state,
                               const TextInputStyle& style);
KeyboardInputState TextInputToKeyboardInput(const TextInputState& state);
void ApplyKeyboardInputToTextInput(TextInputState* state, const KeyboardInputState& input_state);
void DrawTextInput(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   int origin_x,
                   int origin_y,
                   const TextInputState& state,
                   const TextInputStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TEXT_INPUT_H_
