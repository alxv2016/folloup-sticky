#include "notes_page_coordinator.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "project_assets.h"

namespace {

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingMetadata;
using recording_archive_service::RecordingTag;

bool IsNotesTag(RecordingTag tag)
{
    // Notes groups Note + Idea; Task lives on the (future) Todos page.
    return tag == RecordingTag::kNote || tag == RecordingTag::kIdea;
}

int64_t EntryUnixSeconds(const RecordingEntry& entry)
{
    if (entry.metadata.created_unix_seconds > 0) {
        return entry.metadata.created_unix_seconds;
    }
    return entry.modified_unix_seconds;
}

std::string TrimTranscript(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string FormatDateLabel(const RecordingMetadata& metadata)
{
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(metadata.created_local_date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        std::time_t stamp = std::mktime(&tm);
        if (stamp != static_cast<std::time_t>(-1)) {
            std::tm local = {};
            localtime_r(&stamp, &local);
            char buffer[24] = {};
            if (std::strftime(buffer, sizeof(buffer), "%a %b %d", &local) > 0) {
                return buffer;
            }
        }
    }
    return metadata.created_local_date.empty() ? "Today" : metadata.created_local_date;
}

std::string FormatTimeLabel(const RecordingEntry& entry)
{
    if (entry.metadata.time_valid && entry.metadata.created_unix_seconds > 0) {
        std::time_t stamp = static_cast<std::time_t>(entry.metadata.created_unix_seconds);
        std::tm local = {};
        localtime_r(&stamp, &local);
        char buffer[16] = {};
        if (std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local) > 0) {
            std::string text = buffer;
            if (text.size() > 1 && text.front() == '0') {
                text.erase(0, 1);
            }
            return text;
        }
    }
    return "--:--";
}

std::string FormatDurationLabel(uint32_t duration_ms)
{
    const uint32_t seconds = duration_ms / 1000U;
    if (seconds < 60U) {
        return std::to_string(seconds) + "s";
    }
    return std::to_string(seconds / 60U) + "m";
}

std::string TagText(RecordingTag tag)
{
    switch (tag) {
        case RecordingTag::kTask:
            return "Task";
        case RecordingTag::kIdea:
            return "Idea";
        case RecordingTag::kNote:
        default:
            return "Note";
    }
}

const EmbeddedImageAsset* PinIcon()
{
    return project_assets::GetIcon(EmbeddedIconId::kPin);
}

}  // namespace

NotesPageCoordinator::NotesPageCoordinator() = default;

void NotesPageCoordinator::BuildGroups(const std::vector<RecordingEntry>& recordings)
{
    timeline_groups_.clear();

    std::vector<const RecordingEntry*> sorted;
    sorted.reserve(recordings.size());
    for (const RecordingEntry& entry : recordings) {
        if (IsNotesTag(entry.metadata.tag)) {
            sorted.push_back(&entry);
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const RecordingEntry* a, const RecordingEntry* b) {
        return EntryUnixSeconds(*a) > EntryUnixSeconds(*b);
    });

    for (const RecordingEntry* entry_ptr : sorted) {
        const RecordingEntry& entry = *entry_ptr;
        const std::string date_key = !entry.metadata.created_local_date.empty()
                                         ? entry.metadata.created_local_date
                                         : FormatDateLabel(entry.metadata);

        int group_index = -1;
        for (size_t index = 0; index < timeline_groups_.size(); ++index) {
            if (timeline_groups_[index].date_key == date_key) {
                group_index = static_cast<int>(index);
                break;
            }
        }
        if (group_index < 0) {
            timeline_groups_.push_back({date_key, FormatDateLabel(entry.metadata), {}});
            group_index = static_cast<int>(timeline_groups_.size()) - 1;
        }

        const std::string transcript = TrimTranscript(entry.transcript_text);
        const bool has_transcription = entry.metadata.has_transcript && !transcript.empty();

        TimelineEntry timeline_entry = {};
        timeline_entry.recording_id = entry.recording_id;
        timeline_entry.recording_path = entry.recording_path;
        timeline_entry.follow_up = entry.metadata.follow_up;
        timeline_entry.follow_up_completed = entry.metadata.follow_up_completed;
        timeline_entry.item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        timeline_entry.item.header.tag_icon_asset = entry.metadata.follow_up ? PinIcon() : nullptr;
        timeline_entry.item.header.time_text = FormatTimeLabel(entry);
        timeline_entry.item.header.minute_seconds_text =
            FormatDurationLabel(entry.metadata.duration_ms);
        timeline_entry.item.header.tag_text = TagText(entry.metadata.tag);
        timeline_entry.item.body_text = has_transcription ? transcript : "Audio only note.";
        timeline_entry.item.accessory.kind = epaper_ui::ListItemAccessoryKind::kNone;
        timeline_groups_[static_cast<size_t>(group_index)].entries.push_back(
            std::move(timeline_entry));
    }

    if (timeline_groups_.empty()) {
        timeline_groups_.push_back({"today", "Today", {}});
    }
}

void NotesPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    item_list_active_ = false;
    active_group_index_ = -1;
    selected_recording_id_.clear();
    BuildGroups(recordings);
    navigation_model_ = page_navigation::BuildNotesPageNavigationModel(TimelineGroupCount());
    focus_.Configure(navigation_model_.item_count, 0);
    item_focus_.Configure(0, 0);
    visible_group_index_ = TimelineGroupCount() > 0 ? 0 : -1;
}

void NotesPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
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
    navigation_model_ = page_navigation::BuildNotesPageNavigationModel(TimelineGroupCount());
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

int NotesPageCoordinator::FocusedTimelineGroupIndex() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    if (item == nullptr ||
        item->section != page_navigation::NavigationItemSection::kNotesPageTimelineGroups ||
        item->role != page_navigation::NavigationItemRole::kNotesPageTimelineGroup) {
        return -1;
    }
    return item->item_index >= 0 && item->item_index < TimelineGroupCount() ? item->item_index : -1;
}

void NotesPageCoordinator::UpdateSelectedRecordingId()
{
    const TimelineEntry* entry = SelectedEntry();
    selected_recording_id_ = entry != nullptr ? entry->recording_id : std::string();
}

bool NotesPageCoordinator::MoveFocus(int delta)
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

bool NotesPageCoordinator::SetFocusIndex(int index)
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

bool NotesPageCoordinator::EnterFocusedGroup()
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

bool NotesPageCoordinator::ExitItemList()
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

bool NotesPageCoordinator::FocusGroupChip(int group_index)
{
    if (group_index < 0 || group_index >= TimelineGroupCount()) {
        return false;
    }
    ExitItemList();
    return SetFocusIndex(group_index);
}

bool NotesPageCoordinator::EnterGroupItem(int group_index, int item_index)
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

const NotesPageCoordinator::TimelineEntry* NotesPageCoordinator::FindEntry(
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

bool NotesPageCoordinator::FocusRecording(const std::string& recording_id, bool activate_item_list)
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

bool NotesPageCoordinator::SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
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

const NotesPageCoordinator::TimelineEntry* NotesPageCoordinator::SelectedEntry() const
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

bool NotesPageCoordinator::HasOnlyEmptyGroup() const
{
    return timeline_groups_.size() == 1 && timeline_groups_.front().entries.empty();
}

bool NotesPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::NotesPageState NotesPageCoordinator::BuildState() const
{
    epaper_ui::NotesPageState state = {};
    state.title_text = "Notes";
    state.navigation_focus_index = focus_.index();

    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = "Notes";
    timeline.empty_state_text = "No notes available";
    timeline.empty_state_icon_asset = project_assets::GetIcon(EmbeddedIconId::kIdea);
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
