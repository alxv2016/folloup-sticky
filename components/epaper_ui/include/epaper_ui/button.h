#ifndef EPAPER_UI_BUTTON_H_
#define EPAPER_UI_BUTTON_H_

#include <cstdint>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class ButtonVariant : uint8_t {
    kDefault,
    kPrimary,
};

struct ButtonState {
    std::string_view label_text = {};
    bool selected = false;

    bool operator==(const ButtonState& other) const = default;
};

struct ButtonStyle {
    ButtonVariant variant = ButtonVariant::kDefault;
    design::TypographyRole role = design::TypographyRole::kLabelMedium;
    uint8_t background_color = design::status_bar::kBackgroundColor;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    int width = 0;
    int min_width = design::button::kMinWidth;
    int height = design::button::kHeight;
    int horizontal_padding = design::button::kHorizontalPadding;
    bool center_label = false;
    int border_thickness = design::button::kBorderThickness;
    int stroke_thickness = design::button::kStrokeThickness;
};

UiRect ButtonBounds(int origin_x, int origin_y, const ButtonState& state, const ButtonStyle& style);
void DrawButton(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int origin_x,
                int origin_y,
                const ButtonState& state,
                const ButtonStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BUTTON_H_
