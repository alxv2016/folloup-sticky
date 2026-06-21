#ifndef EPAPER_UI_MIC_STATUS_H_
#define EPAPER_UI_MIC_STATUS_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

struct MicStatusState {
    std::string label_text = {};
    bool focused = false;
};

struct MicStatusStyle {
    design::TypographyRole label_role = design::TypographyRole::kLabelMedium;
    uint8_t background_color = design::color::kWhite;
    uint8_t focused_background_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    uint8_t icon_color = design::color::kBlack;
    uint8_t focused_icon_color = design::color::kWhite;
    uint8_t text_color = design::color::kBlack;
    uint8_t focused_text_color = design::color::kWhite;
    int size = design::mic_status::kSize;
    int icon_size = design::mic_status::kIconSize;
    int border_thickness = design::mic_status::kBorderThickness;
    int label_gap = design::mic_status::kLabelGap;
    int trailing_padding = design::mic_status::kTrailingPadding;
    bool outline_idle_icon = false;
};

UiRect MicStatusBounds(int origin_x,
                       int origin_y,
                       const MicStatusState& state,
                       const MicStatusStyle& style);
void DrawMicStatus(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   int origin_x,
                   int origin_y,
                   const MicStatusState& state,
                   const MicStatusStyle& style,
                   const EmbeddedImageAsset* idle_icon,
                   const EmbeddedImageAsset* focused_icon);

}  // namespace epaper_ui

#endif  // EPAPER_UI_MIC_STATUS_H_
