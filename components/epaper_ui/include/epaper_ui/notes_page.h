#ifndef EPAPER_UI_NOTES_PAGE_H_
#define EPAPER_UI_NOTES_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/global_footer.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/timeline_list.h"

namespace epaper_ui {

struct NotesPageState {
    int navigation_focus_index = -1;
    std::string title_text = "Notes";
    TimelineListState timeline = {};

    bool operator==(const NotesPageState& other) const = default;
};

enum class NotesTimelineHitKind : uint8_t {
    kNone = 0,
    kGroupChip,
    kItem,
};

// Result of resolving a touch against the timeline: either a group's date chip or an item row.
struct NotesTimelineHit {
    NotesTimelineHitKind kind = NotesTimelineHitKind::kNone;
    int group_index = -1;
    int item_index = -1;
};

NotesTimelineHit HitTestNotesTimeline(int portrait_width,
                                      int portrait_height,
                                      const NotesPageState& state,
                                      int x,
                                      int y);

void DrawNotesPage(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const NotesPageState& state,
                   const StatusBarState& status_bar_state,
                   const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_NOTES_PAGE_H_
