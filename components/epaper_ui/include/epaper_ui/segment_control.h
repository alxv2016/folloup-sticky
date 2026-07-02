#ifndef EPAPER_UI_SEGMENT_CONTROL_H_
#define EPAPER_UI_SEGMENT_CONTROL_H_

#include <array>
#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/button.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

inline constexpr int kSegmentControlNoSelection = -1;
inline constexpr int kSegmentControlMinSegmentCount = 2;
inline constexpr int kSegmentControlMaxSegmentCount = 3;
inline constexpr int kSegmentControlDefaultSegmentCount = 2;

// A pill of 2-3 mutually exclusive segments (e.g. Notes / Todos). `active` means the control is
// "entered" and UP/DOWN cycles segments; `focused` just draws the focus ring.
struct SegmentControlState {
    std::array<std::string, kSegmentControlMaxSegmentCount> labels = {"", "", ""};
    int segment_count = kSegmentControlDefaultSegmentCount;
    int selected_index = 0;
    bool focused = false;
    bool active = false;
    bool focus_ring_visible = true;

    bool operator==(const SegmentControlState& other) const = default;
};

struct SegmentControlStyle {
    int width = 0;
    int field_height = design::segment_control::kFieldHeight;
    int internal_padding = design::segment_control::kInternalPadding;
    int segment_gap = design::segment_control::kButtonGap;
    int border_thickness = design::segment_control::kBorderThickness;
    int focus_ring_thickness = design::segment_control::kFocusRingThickness;
    int focus_gap = design::segment_control::kFocusGap;
    uint8_t background_color = design::color::kGrayLight;
    uint8_t border_color = design::color::kGrayLight;
    uint8_t inactive_focus_ring_color = design::color::kBlack;
    uint8_t active_focus_ring_color = design::color::kGrayDark;
    uint8_t focus_gap_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kGrayDark;
    uint8_t active_selected_background_color = design::color::kBlack;
    ButtonStyle button = {
        .role = design::TypographyRole::kLabelMedium,
        .background_color = design::color::kGrayLight,
        .selected_background_color = design::color::kGrayDark,
        .border_color = design::color::kBlack,
        .text_color = design::color::kBlack,
        .selected_text_color = design::color::kWhite,
        .width = 0,
        .min_width = 0,
        .height = design::segment_control::kButtonHeight,
        .horizontal_padding = design::segment_control::kButtonHorizontalPadding,
        .center_label = true,
        .border_thickness = design::segment_control::kButtonBorderThickness,
        .stroke_thickness = design::button::kStrokeThickness,
    };
};

int SegmentControlVisibleCount(const SegmentControlState& state);
UiRect SegmentControlBounds(int origin_x, int origin_y, const SegmentControlStyle& style);
UiRect SegmentControlSegmentBounds(int origin_x,
                                   int origin_y,
                                   const SegmentControlState& state,
                                   const SegmentControlStyle& style,
                                   int index);
bool HitTestSegmentControlSegment(int origin_x,
                                  int origin_y,
                                  const SegmentControlState& state,
                                  const SegmentControlStyle& style,
                                  int x,
                                  int y,
                                  int* index);
void DrawSegmentControl(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int origin_x,
                        int origin_y,
                        const SegmentControlState& state,
                        const SegmentControlStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SEGMENT_CONTROL_H_
