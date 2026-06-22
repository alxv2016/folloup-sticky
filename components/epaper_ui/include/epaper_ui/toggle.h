#ifndef EPAPER_UI_TOGGLE_H_
#define EPAPER_UI_TOGGLE_H_

#include <cstdint>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

enum class ToggleVisualState : uint8_t {
    kOff,
    kOn,
    kFocusOff,
    kFocusOn,
};

struct ToggleStyle {
    uint8_t track_color = design::color::kGrayLight;
    uint8_t track_border_color = design::color::kBlack;
    uint8_t thumb_off_fill_color = design::color::kWhite;
    uint8_t thumb_off_border_color = design::color::kBlack;
    uint8_t thumb_on_fill_color = design::color::kBlack;
    uint8_t focus_ring_color = design::color::kBlack;
    uint8_t focus_gap_color = design::color::kWhite;
    int width = design::toggle::kWidth;
    int height = design::toggle::kHeight;
    int thumb_size = design::toggle::kThumbSize;
    int thumb_inset = design::toggle::kThumbInset;
    int track_border_thickness = 1;
    int thumb_border_thickness = design::toggle::kThumbBorderThickness;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
};

UiRect ToggleBounds(int origin_x, int origin_y, const ToggleStyle& style);
void DrawToggle(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int origin_x,
                int origin_y,
                ToggleVisualState state,
                const ToggleStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TOGGLE_H_
