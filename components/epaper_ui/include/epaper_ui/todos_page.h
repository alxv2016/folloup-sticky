#ifndef EPAPER_UI_TODOS_PAGE_H_
#define EPAPER_UI_TODOS_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/global_footer.h"
#include "epaper_ui/notes_page.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/timeline_list.h"

namespace epaper_ui {

struct TodosPageState {
    int navigation_focus_index = -1;
    std::string title_text = "Todos";
    TimelineListState timeline = {};

    bool operator==(const TodosPageState& other) const = default;
};

// Todos and Notes share the timeline touch model; reuse its hit result type.
using TodosTimelineHit = NotesTimelineHit;

TodosTimelineHit HitTestTodosTimeline(int portrait_width,
                                      int portrait_height,
                                      const TodosPageState& state,
                                      int x,
                                      int y);

void DrawTodosPage(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const TodosPageState& state,
                   const StatusBarState& status_bar_state,
                   const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TODOS_PAGE_H_
