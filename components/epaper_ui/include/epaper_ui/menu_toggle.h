#ifndef EPAPER_UI_MENU_TOGGLE_H_
#define EPAPER_UI_MENU_TOGGLE_H_

#include <cstdint>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/toggle.h"

namespace epaper_ui {

struct MenuToggleState {
    std::string_view label_text = {};
    ToggleVisualState toggle_state = ToggleVisualState::kOff;
};

struct MenuToggleStyle {
    design::TypographyRole role = design::TypographyRole::kLabelLargeBlack;
    uint8_t background_color = design::color::kWhite;
    uint8_t text_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int height = design::menu_toggle::kHeight;
    int horizontal_padding = design::menu_toggle::kHorizontalPadding;
    int control_gap = design::menu_toggle::kControlGap;
    int bottom_border_thickness = design::menu_toggle::kBottomBorderThickness;
    ToggleStyle toggle = {};
};

UiRect MenuToggleBounds(int origin_x, int origin_y, const MenuToggleStyle& style);
void DrawMenuToggle(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const MenuToggleState& state,
                    const MenuToggleStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_MENU_TOGGLE_H_
