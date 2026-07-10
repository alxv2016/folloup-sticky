#ifndef EPAPER_UI_CAROUSEL_H_
#define EPAPER_UI_CAROUSEL_H_

#include <cstdint>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/button.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A full-page (below the status bar) slide container. It owns the "chrome": pagination dots on the
// bottom-left and Close / Prev / Next controls on the bottom-right. Slide content is NOT drawn by
// this primitive -- the page renders the active slide into CarouselContentBounds() (the "slot").
struct CarouselState {
    int slide_count = 0;
    int active_index = 0;
    bool show_close = true;  // hide the Close button on non-final slides
    bool close_selected = false;
    bool prev_selected = false;
    bool next_selected = false;

    bool operator==(const CarouselState& other) const = default;
};

struct CarouselStyle {
    int padding = design::carousel::kPadding;
    int dot_diameter = design::carousel::kDotDiameter;
    int dot_gap = design::carousel::kDotGap;
    int control_gap = design::carousel::kControlGap;
    int content_control_gap = design::carousel::kContentControlGap;
    int icon_button_size = design::carousel::kIconButtonSize;
    int icon_size = design::carousel::kIconSize;
    // Touch targets are inflated by these margins (and extended down to the panel bottom) so
    // slightly-low taps and the gaps between controls still register. Mirrors the global footer's
    // asymmetric slop (the controls share the footer's bottom y-band). Does not affect the drawn size.
    int touch_slop_x = design::carousel::kTouchSlopX;
    int touch_slop_y = design::carousel::kTouchSlopY;
    uint8_t active_dot_color = design::carousel::kActiveDotColor;
    uint8_t inactive_dot_color = design::carousel::kInactiveDotColor;
    uint8_t disabled_control_color = design::carousel::kDisabledControlColor;
    std::string_view close_label = "Close";
    // Compact text button (auto-fits the label rather than the wide default min width).
    ButtonStyle close_button = {
        .min_width = 0,
    };
};

// Prev is disabled on the first slide; Next is disabled on the last slide (no wrap-around).
inline bool CarouselPrevDisabled(const CarouselState& state)
{
    return state.active_index <= 0;
}
inline bool CarouselNextDisabled(const CarouselState& state)
{
    return state.slide_count <= 0 || state.active_index >= state.slide_count - 1;
}

// Resolved on-screen rectangles of the three controls (for hit-testing / diagnostics).
struct CarouselControlRects {
    UiRect close = {};
    UiRect prev = {};
    UiRect next = {};
};
CarouselControlRects CarouselControlBounds(int portrait_width,
                                           int portrait_height,
                                           const CarouselState& state,
                                           const CarouselStyle& style);

// The whole carousel region: full width, from below the status bar to the bottom of the panel.
UiRect CarouselBounds(int portrait_width, int portrait_height, const CarouselStyle& style);
// The content slot the page draws the active slide into (inside the padding, above the controls).
UiRect CarouselContentBounds(int portrait_width, int portrait_height, const CarouselStyle& style);

// Touch hit-tests. Prev/Next return false when disabled so a tap at the end is a no-op.
bool HitTestCarouselClose(int portrait_width,
                          int portrait_height,
                          const CarouselState& state,
                          const CarouselStyle& style,
                          int x,
                          int y);
bool HitTestCarouselPrev(int portrait_width,
                         int portrait_height,
                         const CarouselState& state,
                         const CarouselStyle& style,
                         int x,
                         int y);
bool HitTestCarouselNext(int portrait_width,
                         int portrait_height,
                         const CarouselState& state,
                         const CarouselStyle& style,
                         int x,
                         int y);

// Draw the chrome (dots + Close / Prev / Next). Slide content is the caller's responsibility.
void DrawCarousel(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const CarouselState& state,
                  const CarouselStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_CAROUSEL_H_
