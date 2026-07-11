#ifndef EPAPER_UI_SCROLL_CONTAINER_H_
#define EPAPER_UI_SCROLL_CONTAINER_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A bordered panel that shows wrapped text with a percent-based scroll position and a scrollbar.
// When content_text is empty it centers an empty-state icon + message instead. `active` means it
// is "entered" and UP/DOWN scroll; `focused` just draws the focus ring.
struct ScrollContainerState {
    std::string content_text = {};
    std::string empty_state_message = {};
    bool focused = false;
    bool active = false;
    bool focus_ring_visible = true;
    int scroll_position_percent = 0;

    bool operator==(const ScrollContainerState& other) const = default;
};

struct ScrollContainerStyle {
    design::TypographyRole text_role = design::TypographyRole::kBody;
    design::TypographyRole empty_state_role = design::TypographyRole::kLabelSmall;
    uint8_t panel_background_color = design::color::kWhite;
    uint8_t panel_border_color = design::color::kGrayLight;
    uint8_t text_color = design::color::kBlack;
    uint8_t empty_state_text_color = design::color::kBlack;
    uint8_t empty_state_icon_color = design::color::kBlack;
    uint8_t inactive_focus_ring_color = design::color::kBlack;
    uint8_t active_focus_ring_color = design::color::kGrayDark;
    uint8_t focus_gap_color = design::color::kWhite;
    uint8_t scrollbar_track_color = design::color::kWhite;
    uint8_t scrollbar_inactive_color = design::color::kGrayLight;
    uint8_t scrollbar_focused_color = design::color::kBlack;
    uint8_t scrollbar_border_color = design::color::kBlack;
    int width = 0;
    int panel_height = 0;
    int panel_border_thickness = design::scroll_container::kPanelBorderThickness;
    int content_padding = design::scroll_container::kContentPadding;
    int scrollbar_gap = design::scroll_container::kScrollbarGap;
    int scrollbar_width = design::scroll_container::kScrollbarWidth;
    int scrollbar_border_thickness = design::scroll_container::kScrollbarBorderThickness;
    int focus_ring_thickness = design::scroll_container::kFocusRingThickness;
    int focus_gap = design::scroll_container::kFocusGap;
    int line_gap = design::scroll_container::kLineGap;
    int min_thumb_height = design::scroll_container::kMinThumbHeight;
    int empty_state_gap = design::empty_state::kGap;
    int empty_state_icon_size = design::empty_state::kIconSize;
    int empty_state_stroke_thickness = design::button::kStrokeThickness;
};

UiRect ScrollContainerPanelBounds(int origin_x, int origin_y, const ScrollContainerStyle& style);
UiRect ScrollContainerContentViewportBounds(int origin_x,
                                            int origin_y,
                                            const ScrollContainerStyle& style);
bool ScrollContainerHasOverflow(int origin_x,
                                int origin_y,
                                const ScrollContainerState& state,
                                const ScrollContainerStyle& style);
void DrawScrollContainer(uint8_t* framebuffer,
                         int raw_width,
                         int raw_height,
                         int portrait_width,
                         int portrait_height,
                         int origin_x,
                         int origin_y,
                         const ScrollContainerState& state,
                         const ScrollContainerStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SCROLL_CONTAINER_H_
