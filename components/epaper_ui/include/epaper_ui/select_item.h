#ifndef EPAPER_UI_SELECT_ITEM_H_
#define EPAPER_UI_SELECT_ITEM_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct SelectItemState {
    std::string label_text = {};
    bool selected = false;
    bool checked = false;
};

struct SelectItemStyle {
    design::TypographyRole role = design::TypographyRole::kLabelMedium;
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t icon_color = design::color::kBlack;
    uint8_t selected_icon_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int height = design::network_item::kHeight;
    int horizontal_padding = design::network_item::kHorizontalPadding;
    int icon_size = design::network_item::kIconSize;
    int bottom_border_thickness = design::network_item::kBottomBorderThickness;
};

UiRect SelectItemBounds(int origin_x, int origin_y, const SelectItemStyle& style);
void DrawSelectItem(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const SelectItemState& state,
                    const SelectItemStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SELECT_ITEM_H_
