#ifndef EPAPER_UI_CURRENT_DATE_H_
#define EPAPER_UI_CURRENT_DATE_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A single line: weekday, a short divider, then the date (e.g. "Monday — Jun 24, 2026").
struct CurrentDateState {
    std::string weekday_text = {};
    std::string date_text = {};

    bool operator==(const CurrentDateState& other) const = default;
};

struct CurrentDateStyle {
    design::TypographyRole weekday_role = design::TypographyRole::kLabelSmallBlack;
    design::TypographyRole date_role = design::TypographyRole::kLabelSmallBold;
    uint8_t text_color = design::color::kBlack;
    uint8_t separator_color = design::color::kBlack;
    int segment_gap = design::current_date::kSegmentGap;
    int separator_width = design::current_date::kSeparatorWidth;
    int separator_height = design::current_date::kSeparatorHeight;
};

UiRect CurrentDateBounds(int origin_x,
                         int origin_y,
                         const CurrentDateState& state,
                         const CurrentDateStyle& style);
void DrawCurrentDate(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const CurrentDateState& state,
                     const CurrentDateStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_CURRENT_DATE_H_
