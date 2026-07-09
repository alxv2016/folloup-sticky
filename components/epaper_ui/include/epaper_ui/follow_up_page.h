#ifndef EPAPER_UI_FOLLOW_UP_PAGE_H_
#define EPAPER_UI_FOLLOW_UP_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/global_footer.h"
#include "epaper_ui/notes_page.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/timeline_list.h"

namespace epaper_ui {

struct FollowUpPageState {
    int navigation_focus_index = -1;
    std::string title_text = "Follow up";
    TimelineListState timeline = {};

    bool operator==(const FollowUpPageState& other) const = default;
};

// Follow-up, Notes and Todos share the timeline touch model; reuse its hit result type.
using FollowUpTimelineHit = NotesTimelineHit;

FollowUpTimelineHit HitTestFollowUpTimeline(int portrait_width,
                                            int portrait_height,
                                            const FollowUpPageState& state,
                                            int x,
                                            int y);

void DrawFollowUpPage(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const FollowUpPageState& state,
                      const StatusBarState& status_bar_state,
                      const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_FOLLOW_UP_PAGE_H_
