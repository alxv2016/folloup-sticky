#ifndef EPAPER_UI_BUTTON_ICON_H_
#define EPAPER_UI_BUTTON_ICON_H_

#include <cstdint>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

struct ButtonIconState {
    const EmbeddedImageAsset* asset = nullptr;
    bool selected = false;
};

struct ButtonIconStyle {
    uint8_t background_color = design::status_bar::kBackgroundColor;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    uint8_t icon_color = design::color::kBlack;
    uint8_t selected_icon_color = design::color::kWhite;
    int size = design::button_icon::kSize;
    int icon_size = design::button_icon::kIconSize;
    int border_thickness = design::button_icon::kBorderThickness;
    int stroke_thickness = design::button::kStrokeThickness;
    bool outline_icon_when_unselected = true;
};

UiRect ButtonIconBounds(int origin_x, int origin_y, const ButtonIconStyle& style);
void DrawButtonIcon(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const ButtonIconState& state,
                    const ButtonIconStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BUTTON_ICON_H_
