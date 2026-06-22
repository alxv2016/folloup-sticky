#ifndef EPAPER_UI_PROGRESS_BAR_H_
#define EPAPER_UI_PROGRESS_BAR_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct ProgressBarState {
    std::string label_text = {};
    std::string status_text = {};
    int progress_percent = 0;
};

struct ProgressBarStyle {
    design::TypographyRole label_role = design::TypographyRole::kLabelSmall;
    design::TypographyRole status_role = design::TypographyRole::kLabelSmall;
    uint8_t track_color = design::status_bar::kBackgroundColor;
    uint8_t fill_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    int bar_height = design::progress_bar::kBarHeight;
    int meta_gap = design::progress_bar::kMetaGap;
    int border_thickness = design::progress_bar::kBorderThickness;
    int width = 0;
};

UiRect ProgressBarBounds(int origin_x,
                         int origin_y,
                         const ProgressBarState& state,
                         const ProgressBarStyle& style);
void DrawProgressBar(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const ProgressBarState& state,
                     const ProgressBarStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_PROGRESS_BAR_H_
