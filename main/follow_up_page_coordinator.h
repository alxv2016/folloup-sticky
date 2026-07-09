#ifndef FOLLOW_UP_PAGE_COORDINATOR_H_
#define FOLLOW_UP_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/follow_up_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

// Owns the Follow-up timeline: a cross-tag view of every recording flagged follow_up, on the same
// two-level focus model as Notes/Todos. Rows carry a checkbox (a tap-to-complete affordance);
// completing a follow-up clears the flag so it leaves the list.
class FollowUpPageCoordinator {
public:
    struct TimelineEntry {
        epaper_ui::ListItemState item = {};
        std::string recording_id = {};
        std::string recording_path = {};
        bool follow_up = false;
        bool follow_up_completed = false;
    };
    struct TimelineGroup {
        std::string date_key = {};
        std::string label_text = {};
        std::vector<TimelineEntry> entries = {};
    };

    FollowUpPageCoordinator();

    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool EnterFocusedGroup();
    bool ExitItemList();
    bool FocusGroupChip(int group_index);
    bool EnterGroupItem(int group_index, int item_index);
    bool FocusRecording(const std::string& recording_id, bool activate_item_list);
    bool SetEntryChecked(const std::string& recording_id, bool checked);

    epaper_ui::FollowUpPageState BuildState() const;

    int TimelineGroupCount() const { return static_cast<int>(timeline_groups_.size()); }
    int FocusedTimelineGroupIndex() const;
    const TimelineEntry* SelectedEntry() const;
    bool HasOnlyEmptyGroup() const;
    bool item_list_active() const { return item_list_active_; }
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void BuildGroups(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void UpdateSelectedRecordingId();
    const TimelineEntry* FindEntry(const std::string& recording_id, int* group_index,
                                   int* entry_index) const;

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildFollowUpPageNavigationModel(0);
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus item_focus_{0, 0};
    std::vector<TimelineGroup> timeline_groups_ = {};
    bool item_list_active_ = false;
    int active_group_index_ = -1;
    int visible_group_index_ = -1;
    std::string selected_recording_id_ = {};
};

#endif  // FOLLOW_UP_PAGE_COORDINATOR_H_
