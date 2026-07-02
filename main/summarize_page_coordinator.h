#ifndef SUMMARIZE_PAGE_COORDINATOR_H_
#define SUMMARIZE_PAGE_COORDINATOR_H_

#include <array>
#include <string>

#include "epaper_ui/summarize_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "summary_service.h"

// Owns the Summarize page's focus + view state. It shows the cached Notes/Todos summary for the
// selected segment in a scroll container, and drives the "Get summary" action.
class SummarizePageCoordinator {
public:
    SummarizePageCoordinator();

    // (Re)enter the page: focus the segment control, collapse any entered control.
    void PrepareForShow();

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    page_navigation::NavigationItemRole FocusedRole() const;

    bool EnterSegmentControl();
    bool ExitSegmentControl();
    bool EnterScrollContainer();
    bool ExitScrollContainer();
    // Select a segment directly (touch). Returns true when the selection changed.
    bool SelectSegment(int index);

    bool segment_control_active() const { return segment_control_active_; }
    bool scroll_container_active() const { return scroll_container_active_; }
    int selected_segment_index() const { return selected_segment_index_; }

    epaper_ui::SummarizePageState BuildState(bool gemini_ready,
                                             const summary_service::Snapshot& summary_snapshot) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    std::string BuildContentText(const summary_service::Snapshot& summary_snapshot) const;
    std::string BuildEmptyStateMessage(bool gemini_ready,
                                       const summary_service::Snapshot& summary_snapshot) const;

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildSummarizePageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus segment_focus_{epaper_ui::kSegmentControlDefaultSegmentCount, 0};
    int selected_segment_index_ = 0;
    bool segment_control_active_ = false;
    bool scroll_container_active_ = false;
    std::array<int, epaper_ui::kSegmentControlDefaultSegmentCount> scroll_position_percent_ = {0, 0};
};

#endif  // SUMMARIZE_PAGE_COORDINATOR_H_
