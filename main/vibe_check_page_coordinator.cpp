#include "vibe_check_page_coordinator.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "esp_random.h"
#include "generated_epaper_icons.h"

namespace {

using page_navigation::NavigationItemRole;
using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingMetadata;
using recording_archive_service::RecordingTag;

constexpr const char* kMessageText =
    "Some thoughts are passing vibes. Drop it, or follow up on what still hits.";
constexpr const char* kEmptyStateMessage = "Get the ball rolling! Capture some ideas!";
constexpr const char* kAudioOnlyMessage = "Audio only note...";

epaper_ui::VibeCardActionSelection ActionSelectionForIndex(int index)
{
    switch (index) {
        case 0:
            return epaper_ui::VibeCardActionSelection::kRefresh;
        case 1:
            return epaper_ui::VibeCardActionSelection::kClose;
        case 2:
            return epaper_ui::VibeCardActionSelection::kCheck;
        default:
            return epaper_ui::VibeCardActionSelection::kNone;
    }
}

// "Mon Jan 3" from the stored YYYY-MM-DD; falls back to the raw date or "Today".
std::string FormatArchiveDateLabel(const RecordingMetadata& metadata)
{
    if (metadata.created_local_date.empty()) {
        return "Today";
    }
    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(metadata.created_local_date.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        std::tm local_tm = {};
        local_tm.tm_year = year - 1900;
        local_tm.tm_mon = month - 1;
        local_tm.tm_mday = day;
        if (std::mktime(&local_tm) != static_cast<time_t>(-1)) {
            char weekday_buffer[8] = {};
            char month_buffer[8] = {};
            if (std::strftime(weekday_buffer, sizeof(weekday_buffer), "%a", &local_tm) > 0 &&
                std::strftime(month_buffer, sizeof(month_buffer), "%b", &local_tm) > 0) {
                char label_buffer[24] = {};
                std::snprintf(label_buffer, sizeof(label_buffer), "%s %s %d", weekday_buffer,
                              month_buffer, day);
                return label_buffer;
            }
        }
    }
    return metadata.created_local_date;
}

std::string FormatArchiveTimeLabel(const RecordingEntry& entry)
{
    if (entry.metadata.time_valid && entry.metadata.created_unix_seconds > 0) {
        time_t epoch_seconds = static_cast<time_t>(entry.metadata.created_unix_seconds);
        std::tm local_tm = {};
        localtime_r(&epoch_seconds, &local_tm);
        char buffer[16] = {};
        if (std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local_tm) > 0) {
            if (buffer[0] == '0') {
                return std::string(buffer + 1);
            }
            return buffer;
        }
    }
    return "--:--";
}

std::string FormatArchiveDurationLabel(uint32_t duration_ms)
{
    const uint32_t total_seconds = duration_ms / 1000U;
    char buffer[16] = {};
    if (total_seconds <= 60U) {
        std::snprintf(buffer, sizeof(buffer), "%02us", static_cast<unsigned>(total_seconds));
        return buffer;
    }
    const uint32_t minutes = total_seconds / 60U;
    std::snprintf(buffer, sizeof(buffer), "%um", static_cast<unsigned>(minutes));
    return buffer;
}

std::string TagTextForRecording(const RecordingMetadata& metadata)
{
    switch (metadata.tag) {
        case RecordingTag::kIdea:
            return "Idea";
        case RecordingTag::kTask:
            return "Task";
        case RecordingTag::kNote:
        default:
            return "Note";
    }
}

std::string TrimTranscriptText(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

}  // namespace

VibeCheckPageCoordinator::VibeCheckPageCoordinator()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kVibeCheckPageCard));
    RebuildCardState();
}

void VibeCheckPageCoordinator::PrepareForShow()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kVibeCheckPageCard));
    card_active_ = false;
    action_focus_.Configure(epaper_ui::kVibeCardActionCount, 0);
    session_initialized_ = false;
    initial_idea_count_ = 0;
}

void VibeCheckPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& entries)
{
    LoadIdeas(entries, !session_initialized_);
}

bool VibeCheckPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    // While the action row is live, UP/DOWN steps through the three actions. Because this
    // device has no dedicated back key, stepping past either edge leaves the card and hands
    // the movement back to page navigation (card <-> footer) so focus can never get trapped.
    if (card_active_ && HasIdeas()) {
        const int next = action_focus_.index() + delta;
        if (next < 0 || next >= epaper_ui::kVibeCardActionCount) {
            card_active_ = false;
            return focus_.Move(delta);
        }
        return action_focus_.SetIndex(next);
    }
    return focus_.Move(delta);
}

bool VibeCheckPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool VibeCheckPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

page_navigation::NavigationItemRole VibeCheckPageCoordinator::FocusedRole() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    return item != nullptr ? item->role : NavigationItemRole::kUnknown;
}

bool VibeCheckPageCoordinator::EnterCard()
{
    if (card_active_ || !HasIdeas()) {
        return false;
    }
    card_active_ = true;
    action_focus_.Configure(epaper_ui::kVibeCardActionCount, 0);
    return true;
}

bool VibeCheckPageCoordinator::EnterCardAtAction(int action_index)
{
    if (!HasIdeas()) {
        return false;
    }
    card_active_ = true;
    action_focus_.Configure(epaper_ui::kVibeCardActionCount,
                            std::max(0, std::min(action_index, epaper_ui::kVibeCardActionCount - 1)));
    return true;
}

bool VibeCheckPageCoordinator::ExitCard()
{
    if (!card_active_) {
        return false;
    }
    card_active_ = false;
    return true;
}

bool VibeCheckPageCoordinator::HasIdeas() const
{
    return FindCurrentIdea() != nullptr;
}

epaper_ui::VibeCardActionSelection VibeCheckPageCoordinator::selected_action() const
{
    return card_active_ && HasIdeas() ? ActionSelectionForIndex(action_focus_.index())
                                      : epaper_ui::VibeCardActionSelection::kNone;
}

bool VibeCheckPageCoordinator::RandomizeIdea()
{
    if (ideas_.empty()) {
        return false;
    }
    const std::string previous = current_recording_id_;
    SelectRandomIdea(true);
    RebuildCardState();
    return !current_recording_id_.empty() && current_recording_id_ != previous;
}

bool VibeCheckPageCoordinator::RemoveCurrentIdea()
{
    if (current_recording_id_.empty()) {
        return false;
    }
    const std::string removed_id = current_recording_id_;
    const auto it = std::remove_if(ideas_.begin(), ideas_.end(),
                                   [&removed_id](const RecordingEntry& entry) {
                                       return entry.recording_id == removed_id;
                                   });
    if (it == ideas_.end()) {
        return false;
    }
    ideas_.erase(it, ideas_.end());
    current_recording_id_.clear();
    SelectRandomIdea(false);
    ResetSessionIfEmpty();
    RebuildCardState();
    if (!HasIdeas()) {
        card_active_ = false;
    }
    return true;
}

epaper_ui::VibeCheckPageState VibeCheckPageCoordinator::BuildState() const
{
    epaper_ui::VibeCheckPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.card = card_state_;
    state.card.focused = IsRoleFocused(NavigationItemRole::kVibeCheckPageCard) || card_active_;
    state.card.footer.selected_action = selected_action();
    state.progress = progress_state_;
    state.message_text = kMessageText;
    return state;
}

void VibeCheckPageCoordinator::LoadIdeas(const std::vector<RecordingEntry>& entries,
                                         bool reset_session)
{
    const std::string previous = current_recording_id_;
    ideas_.clear();
    ideas_.reserve(entries.size());
    for (const RecordingEntry& entry : entries) {
        if (IsIdeaCandidate(entry)) {
            ideas_.push_back(entry);
        }
    }

    if (reset_session) {
        initial_idea_count_ = ideas_.size();
        session_initialized_ = true;
    } else if (ideas_.size() > initial_idea_count_) {
        initial_idea_count_ = ideas_.size();
    }

    current_recording_id_.clear();
    if (!previous.empty()) {
        for (const RecordingEntry& entry : ideas_) {
            if (entry.recording_id == previous) {
                current_recording_id_ = previous;
                break;
            }
        }
    }
    if (current_recording_id_.empty()) {
        SelectRandomIdea(false);
    }
    ResetSessionIfEmpty();
    RebuildCardState();
    if (!HasIdeas()) {
        card_active_ = false;
    }
}

void VibeCheckPageCoordinator::RebuildCardState()
{
    card_state_ = {};
    card_state_.empty_state_icon_asset = &epaper_icons::kIdea;
    card_state_.empty_state_message = kEmptyStateMessage;

    const RecordingEntry* entry = FindCurrentIdea();
    if (entry == nullptr) {
        card_state_.empty = true;
        progress_state_.label_text = "Your ideas";
        progress_state_.status_text = "0/0 ideas";
        progress_state_.progress_percent = 0;
        return;
    }

    card_state_.tag_text = FormatArchiveDateLabel(entry->metadata);
    card_state_.header.icon_asset =
        entry->metadata.has_transcript ? &epaper_icons::kTranscribe : &epaper_icons::kAudio;
    card_state_.header.time_text = FormatArchiveTimeLabel(*entry);
    card_state_.header.minute_seconds_text = FormatArchiveDurationLabel(entry->metadata.duration_ms);
    card_state_.header.tag_text = TagTextForRecording(entry->metadata);
    const std::string transcript = TrimTranscriptText(entry->transcript_text);
    card_state_.body_text =
        entry->metadata.has_transcript && !transcript.empty() ? transcript : kAudioOnlyMessage;

    const size_t remaining = ideas_.size();
    progress_state_.label_text = "Your ideas";
    progress_state_.status_text =
        remaining == 0 ? "0/0 ideas"
                       : std::to_string(remaining) + "/" + std::to_string(initial_idea_count_) +
                             " ideas";
    progress_state_.progress_percent =
        initial_idea_count_ == 0 ? 0
                                 : static_cast<int>((remaining * 100U) / initial_idea_count_);
}

void VibeCheckPageCoordinator::ResetSessionIfEmpty()
{
    if (!ideas_.empty()) {
        return;
    }
    current_recording_id_.clear();
    initial_idea_count_ = 0;
    session_initialized_ = false;
}

void VibeCheckPageCoordinator::SelectRandomIdea(bool avoid_current)
{
    if (ideas_.empty()) {
        current_recording_id_.clear();
        return;
    }
    if (ideas_.size() == 1) {
        current_recording_id_ = ideas_.front().recording_id;
        return;
    }

    const std::string previous = current_recording_id_;
    size_t selected = static_cast<size_t>(esp_random()) % ideas_.size();
    if (avoid_current && !previous.empty() && ideas_[selected].recording_id == previous) {
        selected = (selected + 1) % ideas_.size();
    }
    current_recording_id_ = ideas_[selected].recording_id;
}

const recording_archive_service::RecordingEntry* VibeCheckPageCoordinator::FindCurrentIdea() const
{
    if (current_recording_id_.empty()) {
        return nullptr;
    }
    for (const RecordingEntry& entry : ideas_) {
        if (entry.recording_id == current_recording_id_) {
            return &entry;
        }
    }
    return nullptr;
}

bool VibeCheckPageCoordinator::IsIdeaCandidate(const RecordingEntry& entry)
{
    return entry.metadata.tag == RecordingTag::kIdea && !entry.metadata.follow_up &&
           !entry.metadata.follow_up_completed;
}
