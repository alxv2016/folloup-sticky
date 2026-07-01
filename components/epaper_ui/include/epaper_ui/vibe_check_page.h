#ifndef EPAPER_UI_VIBE_CHECK_PAGE_H_
#define EPAPER_UI_VIBE_CHECK_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/global_footer.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/progress_bar.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/vibe_card.h"

namespace epaper_ui {

struct VibeCheckPageState {
    int navigation_focus_index = -1;
    std::string title_text = "Vibe check";
    VibeCardState card = {};
    ProgressBarState progress = {};
    std::string message_text = {};

    bool operator==(const VibeCheckPageState& other) const = default;
};

// Full card rectangle (used to focus/enter the card by touch).
UiRect VibeCheckCardBounds(int portrait_width,
                           int portrait_height,
                           const VibeCheckPageState& state);
bool HitTestVibeCheckCard(int portrait_width,
                          int portrait_height,
                          const VibeCheckPageState& state,
                          int x,
                          int y);
// Hit-test a footer action button on the card. Returns false when the point misses.
bool HitTestVibeCheckAction(int portrait_width,
                            int portrait_height,
                            const VibeCheckPageState& state,
                            int x,
                            int y,
                            VibeCardActionSelection* action);
void DrawVibeCheckPage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const VibeCheckPageState& state,
                       const StatusBarState& status_bar_state,
                       const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_VIBE_CHECK_PAGE_H_
