#ifndef EPAPER_UI_NETWORK_ITEM_H_
#define EPAPER_UI_NETWORK_ITEM_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class NetworkSignalStrength : uint8_t {
    kMedium = 0,
    kStrong,
};

struct NetworkItemState {
    std::string ssid_text = {};
    bool selected = false;
    bool current_network = false;
    bool private_network = false;
    NetworkSignalStrength signal_strength = NetworkSignalStrength::kStrong;
};

struct NetworkItemStyle {
    design::TypographyRole role = design::TypographyRole::kLabelMedium;
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t icon_color = design::color::kBlack;
    uint8_t selected_icon_color = design::color::kWhite;
    uint8_t selected_content_outline_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int height = design::network_item::kHeight;
    int horizontal_padding = design::network_item::kHorizontalPadding;
    int icon_size = design::network_item::kIconSize;
    int icon_gap = design::network_item::kIconGap;
    int bottom_border_thickness = design::network_item::kBottomBorderThickness;
    int selected_content_stroke_thickness = design::button::kStrokeThickness;
    bool selected_content_outlined = false;
};

UiRect NetworkItemBounds(int origin_x, int origin_y, const NetworkItemStyle& style);
void DrawNetworkItem(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const NetworkItemState& state,
                     const NetworkItemStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_NETWORK_ITEM_H_
