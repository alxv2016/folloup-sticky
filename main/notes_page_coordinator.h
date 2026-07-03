#ifndef NOTES_PAGE_COORDINATOR_H_
#define NOTES_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/notes_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

// Owns the Notes timeline's data + two-level focus: the top level roves date-group chips (and the
// footer), and entering a group activates a second focus level over that group's item rows.
class NotesPageCoordinator {
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

    NotesPageCoordinator();

    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool EnterFocusedGroup();
    bool ExitItemList();
    // Touch helpers: focus a group's chip (collapsing any entered list), or enter a group's item
    // list directly at a given item row.
    bool FocusGroupChip(int group_index);
    bool EnterGroupItem(int group_index, int item_index);
    bool FocusRecording(const std::string& recording_id, bool activate_item_list);
    bool SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                               bool follow_up_completed);

    epaper_ui::NotesPageState BuildState() const;

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
        page_navigation::BuildNotesPageNavigationModel(0);
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus item_focus_{0, 0};
    std::vector<TimelineGroup> timeline_groups_ = {};
    bool item_list_active_ = false;
    int active_group_index_ = -1;
    int visible_group_index_ = -1;
    std::string selected_recording_id_ = {};
};

#endif  // NOTES_PAGE_COORDINATOR_H_
