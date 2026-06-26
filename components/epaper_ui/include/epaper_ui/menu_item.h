#ifndef EPAPER_UI_MENU_ITEM_H_
#define EPAPER_UI_MENU_ITEM_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/badge.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A full-width menu row: label on the left, optional badge on the right, a bottom-border
// separator, and an inverted (black) fill when selected. Used for the dashboard's list menu.
struct MenuItemState {
    std::string label_text = {};
    bool selected = false;
    bool shows_badge = false;
    BadgeState badge = {};

    bool operator==(const MenuItemState& other) const = default;
};

struct MenuItemStyle {
    design::TypographyRole role = design::TypographyRole::kLabelXL;
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int height = design::menu_item::kHeight;
    int horizontal_padding = design::menu_item::kHorizontalPadding;
    int bottom_border_thickness = design::menu_item::kBottomBorderThickness;
    BadgeStyle badge = {};
};

UiRect MenuItemBounds(int origin_x, int origin_y, const MenuItemStyle& style);
void DrawMenuItem(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const MenuItemState& state,
                  const MenuItemStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_MENU_ITEM_H_
