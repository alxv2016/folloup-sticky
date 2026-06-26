#ifndef EPAPER_UI_BADGE_H_
#define EPAPER_UI_BADGE_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A small pill label (e.g. "New"). Solid by default; `inverse` draws a bordered white pill.
struct BadgeState {
    std::string label_text = {};
    bool inverse = false;

    bool operator==(const BadgeState& other) const = default;
};

struct BadgeStyle {
    design::TypographyRole role = design::TypographyRole::kLabelSmall;
    uint8_t background_color = design::color::kBlack;
    uint8_t text_color = design::color::kWhite;
    uint8_t inverse_background_color = design::color::kWhite;
    uint8_t inverse_text_color = design::color::kBlack;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int min_width = design::badge::kMinWidth;
    int height = design::badge::kHeight;
    int border_thickness = design::badge::kBorderThickness;
    int horizontal_padding = design::badge::kHorizontalPadding;
};

UiRect BadgeBounds(int origin_x, int origin_y, const BadgeState& state, const BadgeStyle& style);
void DrawBadge(uint8_t* framebuffer,
               int raw_width,
               int raw_height,
               int portrait_width,
               int portrait_height,
               int origin_x,
               int origin_y,
               const BadgeState& state,
               const BadgeStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_BADGE_H_
