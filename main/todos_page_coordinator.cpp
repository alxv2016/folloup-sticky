#include "todos_page_coordinator.h"

#include <algorithm>

#include "project_assets.h"
#include "timeline_format.h"

namespace {

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingTag;

bool IsTodoTag(RecordingTag tag)
{
    // Todos hold Task recordings; Note + Idea live on the Notes page.
    return tag == RecordingTag::kTask;
}

int64_t EntryUnixSeconds(const RecordingEntry& entry)
{
    if (entry.metadata.created_unix_seconds > 0) {
        return entry.metadata.created_unix_seconds;
    }
    return entry.modified_unix_seconds;
}

const EmbeddedImageAsset* PinIcon()
{
    return project_assets::GetIcon(EmbeddedIconId::kPin);
}

}  // namespace

TodosPageCoordinator::TodosPageCoordinator() = default;

void TodosPageCoordinator::BuildGroups(const std::vector<RecordingEntry>& recordings)
{
    timeline_groups_.clear();

    std::vector<const RecordingEntry*> sorted;
    sorted.reserve(recordings.size());
    for (const RecordingEntry& entry : recordings) {
        if (IsTodoTag(entry.metadata.tag)) {
            sorted.push_back(&entry);
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const RecordingEntry* a, const RecordingEntry* b) {
        return EntryUnixSeconds(*a) > EntryUnixSeconds(*b);
    });

    for (const RecordingEntry* entry_ptr : sorted) {
        const RecordingEntry& entry = *entry_ptr;
        const std::string date_key =
            !entry.metadata.created_local_date.empty()
                ? timeline_format::DateKey(entry.metadata.created_local_date)
                : timeline_format::FormatDateLabel(entry.metadata.created_local_date);

        int group_index = -1;
        for (size_t index = 0; index < timeline_groups_.size(); ++index) {
            if (timeline_groups_[index].date_key == date_key) {
                group_index = static_cast<int>(index);
                break;
            }
        }
        if (group_index < 0) {
            timeline_groups_.push_back(
                {date_key, timeline_format::FormatDateLabel(entry.metadata.created_local_date), {}});
            group_index = static_cast<int>(timeline_groups_.size()) - 1;
        }

        const std::string transcript = timeline_format::TrimTranscript(entry.transcript_text);
        const bool has_transcription = entry.metadata.has_transcript && !transcript.empty();

        TimelineEntry timeline_entry = {};
        timeline_entry.recording_id = entry.recording_id;
        timeline_entry.recording_path = entry.recording_path;
        timeline_entry.follow_up = entry.metadata.follow_up;
        timeline_entry.follow_up_completed = entry.metadata.follow_up_completed;
        timeline_entry.completed = entry.metadata.completed;
        timeline_entry.item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        timeline_entry.item.header.tag_icon_asset = entry.metadata.follow_up ? PinIcon() : nullptr;
        timeline_entry.item.header.time_text = timeline_format::FormatTimeLabel(entry.metadata.time_valid, entry.metadata.created_unix_seconds);
        timeline_entry.item.header.minute_seconds_text =
            timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
        timeline_entry.item.header.tag_text = "Task";
        timeline_entry.item.body_text = has_transcription ? transcript : "Audio only todo.";
        timeline_entry.item.accessory.kind = epaper_ui::ListItemAccessoryKind::kCheckbox;
        timeline_entry.item.accessory.checked = entry.metadata.completed;
        timeline_groups_[static_cast<size_t>(group_index)].entries.push_back(
            std::move(timeline_entry));
    }

    if (timeline_groups_.empty()) {
        timeline_groups_.push_back({"today", "Today", {}});
    }
}

void TodosPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    item_list_active_ = false;
    active_group_index_ = -1;
    selected_recording_id_.clear();
    BuildGroups(recordings);
    navigation_model_ = page_navigation::BuildTodosPageNavigationModel(TimelineGroupCount());
    focus_.Configure(navigation_model_.item_count, 0);
    item_focus_.Configure(0, 0);
    visible_group_index_ = TimelineGroupCount() > 0 ? 0 : -1;
}

void TodosPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    const bool was_active = item_list_active_;
    const std::string selected_id = selected_recording_id_;
    const int focused_group = FocusedTimelineGroupIndex();
    std::string focused_date_key;
    if (focused_group >= 0 && focused_group < TimelineGroupCount()) {
        focused_date_key = timeline_groups_[static_cast<size_t>(focused_group)].date_key;
    }

    item_list_active_ = false;
    active_group_index_ = -1;
    BuildGroups(recordings);
    navigation_model_ = page_navigation::BuildTodosPageNavigationModel(TimelineGroupCount());
    focus_.Configure(navigation_model_.item_count, 0);
    item_focus_.Configure(0, 0);
    visible_group_index_ = TimelineGroupCount() > 0 ? 0 : -1;

    if (was_active && !selected_id.empty() && FocusRecording(selected_id, true)) {
        return;
    }
    if (!focused_date_key.empty()) {
        for (size_t index = 0; index < timeline_groups_.size(); ++index) {
            if (timeline_groups_[index].date_key == focused_date_key) {
                SetFocusIndex(static_cast<int>(index));
                break;
            }
        }
    }
}

int TodosPageCoordinator::FocusedTimelineGroupIndex() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    if (item == nullptr ||
        item->section != page_navigation::NavigationItemSection::kTodosPageTimelineGroups ||
        item->role != page_navigation::NavigationItemRole::kTodosPageTimelineGroup) {
        return -1;
    }
    return item->item_index >= 0 && item->item_index < TimelineGroupCount() ? item->item_index : -1;
}

void TodosPageCoordinator::UpdateSelectedRecordingId()
{
    const TimelineEntry* entry = SelectedEntry();
    selected_recording_id_ = entry != nullptr ? entry->recording_id : std::string();
}

bool TodosPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    if (item_list_active_) {
        if (!item_focus_.Move(delta)) {
            return false;
        }
        visible_group_index_ = active_group_index_;
        UpdateSelectedRecordingId();
        return true;
    }
    if (!focus_.Move(delta)) {
        return false;
    }
    const int group = FocusedTimelineGroupIndex();
    if (group >= 0) {
        visible_group_index_ = group;
    }
    return true;
}

bool TodosPageCoordinator::SetFocusIndex(int index)
{
    if (!focus_.SetIndex(index)) {
        return false;
    }
    const int group = FocusedTimelineGroupIndex();
    if (group >= 0) {
        visible_group_index_ = group;
    }
    return true;
}

bool TodosPageCoordinator::EnterFocusedGroup()
{
    if (item_list_active_) {
        return false;
    }
    const int group = FocusedTimelineGroupIndex();
    if (group < 0 || timeline_groups_[static_cast<size_t>(group)].entries.empty()) {
        return false;
    }
    item_list_active_ = true;
    active_group_index_ = group;
    item_focus_.Configure(
        static_cast<int>(timeline_groups_[static_cast<size_t>(group)].entries.size()), 0);
    visible_group_index_ = group;
    UpdateSelectedRecordingId();
    return true;
}

bool TodosPageCoordinator::ExitItemList()
{
    if (!item_list_active_) {
        return false;
    }
    item_list_active_ = false;
    active_group_index_ = -1;
    item_focus_.Configure(0, 0);
    selected_recording_id_.clear();
    return true;
}

bool TodosPageCoordinator::FocusGroupChip(int group_index)
{
    if (group_index < 0 || group_index >= TimelineGroupCount()) {
        return false;
    }
    ExitItemList();
    return SetFocusIndex(group_index);
}

bool TodosPageCoordinator::EnterGroupItem(int group_index, int item_index)
{
    if (group_index < 0 || group_index >= TimelineGroupCount()) {
        return false;
    }
    const std::vector<TimelineEntry>& entries =
        timeline_groups_[static_cast<size_t>(group_index)].entries;
    if (entries.empty()) {
        return false;
    }
    SetFocusIndex(group_index);
    item_list_active_ = true;
    active_group_index_ = group_index;
    const int clamped = std::clamp(item_index, 0, static_cast<int>(entries.size()) - 1);
    item_focus_.Configure(static_cast<int>(entries.size()), clamped);
    visible_group_index_ = group_index;
    UpdateSelectedRecordingId();
    return true;
}

const TodosPageCoordinator::TimelineEntry* TodosPageCoordinator::FindEntry(
    const std::string& recording_id, int* group_index, int* entry_index) const
{
    if (recording_id.empty()) {
        return nullptr;
    }
    for (size_t g = 0; g < timeline_groups_.size(); ++g) {
        const std::vector<TimelineEntry>& entries = timeline_groups_[g].entries;
        for (size_t e = 0; e < entries.size(); ++e) {
            if (entries[e].recording_id == recording_id) {
                if (group_index != nullptr) {
                    *group_index = static_cast<int>(g);
                }
                if (entry_index != nullptr) {
                    *entry_index = static_cast<int>(e);
                }
                return &entries[e];
            }
        }
    }
    return nullptr;
}

bool TodosPageCoordinator::FocusRecording(const std::string& recording_id, bool activate_item_list)
{
    int group_index = -1;
    int entry_index = -1;
    if (FindEntry(recording_id, &group_index, &entry_index) == nullptr) {
        return false;
    }
    SetFocusIndex(group_index);
    visible_group_index_ = group_index;
    selected_recording_id_ = recording_id;
    if (activate_item_list) {
        item_list_active_ = true;
        active_group_index_ = group_index;
        item_focus_.Configure(
            static_cast<int>(timeline_groups_[static_cast<size_t>(group_index)].entries.size()),
            entry_index);
    }
    return true;
}

bool TodosPageCoordinator::SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                                                 bool follow_up_completed)
{
    for (TimelineGroup& group : timeline_groups_) {
        for (TimelineEntry& entry : group.entries) {
            if (entry.recording_id == recording_id) {
                entry.follow_up = follow_up;
                entry.follow_up_completed = follow_up_completed;
                entry.item.header.tag_icon_asset = follow_up ? PinIcon() : nullptr;
                return true;
            }
        }
    }
    return false;
}

bool TodosPageCoordinator::SetEntryChecked(const std::string& recording_id, bool checked)
{
    for (TimelineGroup& group : timeline_groups_) {
        for (TimelineEntry& entry : group.entries) {
            if (entry.recording_id == recording_id) {
                entry.completed = checked;
                entry.item.accessory.checked = checked;
                return true;
            }
        }
    }
    return false;
}

const TodosPageCoordinator::TimelineEntry* TodosPageCoordinator::SelectedEntry() const
{
    if (!item_list_active_ || active_group_index_ < 0 ||
        active_group_index_ >= TimelineGroupCount()) {
        return nullptr;
    }
    const std::vector<TimelineEntry>& entries =
        timeline_groups_[static_cast<size_t>(active_group_index_)].entries;
    const int index = item_focus_.index();
    if (index < 0 || index >= static_cast<int>(entries.size())) {
        return nullptr;
    }
    return &entries[static_cast<size_t>(index)];
}

bool TodosPageCoordinator::HasOnlyEmptyGroup() const
{
    return timeline_groups_.size() == 1 && timeline_groups_.front().entries.empty();
}

bool TodosPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::TodosPageState TodosPageCoordinator::BuildState() const
{
    epaper_ui::TodosPageState state = {};
    state.title_text = "Todos";
    state.navigation_focus_index = focus_.index();

    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = "Todos";
    timeline.empty_state_text = "No todos available";
    timeline.empty_state_icon_asset = project_assets::GetIcon(EmbeddedIconId::kTaskStart);
    timeline.visible_group_index = visible_group_index_;
    timeline.focused_group_index = FocusedTimelineGroupIndex();
    timeline.active_group_index = item_list_active_ ? active_group_index_ : -1;
    timeline.selected_item_index = item_list_active_ ? item_focus_.index() : -1;
    timeline.groups.reserve(timeline_groups_.size());
    for (const TimelineGroup& group : timeline_groups_) {
        epaper_ui::TimelineGroupState group_state = {};
        group_state.label_text = group.label_text;
        group_state.items.reserve(group.entries.size());
        for (const TimelineEntry& entry : group.entries) {
            group_state.items.push_back(entry.item);
        }
        timeline.groups.push_back(std::move(group_state));
    }
    state.timeline = std::move(timeline);
    return state;
}
