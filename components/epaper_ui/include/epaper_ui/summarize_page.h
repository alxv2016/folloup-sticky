#ifndef EPAPER_UI_SUMMARIZE_PAGE_H_
#define EPAPER_UI_SUMMARIZE_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/scroll_container.h"
#include "epaper_ui/segment_control.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct SummarizePageState {
    int navigation_focus_index = -1;
    std::string title_text = "Summarize";
    SegmentControlState segment_control = {};
    ScrollContainerState scroll_container = {};
    ButtonState get_summary_button = {};

    bool operator==(const SummarizePageState& other) const = default;
};

// Hit-tests for touch. The segment variant also reports which segment index was hit.
bool HitTestSummarizeSegment(int portrait_width,
                             int portrait_height,
                             const SummarizePageState& state,
                             int x,
                             int y,
                             int* segment_index);
bool HitTestSummarizeScrollContainer(int portrait_width,
                                     int portrait_height,
                                     const SummarizePageState& state,
                                     int x,
                                     int y);
bool HitTestSummarizeGetSummaryButton(int portrait_width,
                                      int portrait_height,
                                      const SummarizePageState& state,
                                      int x,
                                      int y);
void DrawSummarizePage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const SummarizePageState& state,
                       const StatusBarState& status_bar_state,
                       const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SUMMARIZE_PAGE_H_
