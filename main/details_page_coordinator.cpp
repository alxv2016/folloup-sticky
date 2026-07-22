#include "details_page_coordinator.h"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "project_assets.h"

namespace {

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingMetadata;
using recording_archive_service::RecordingTag;
using page_navigation::NavigationItemRole;

constexpr int kScrollStepPercent = 10;
constexpr const char* kNoTranscriptMessage = "No transcript available.";

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
    return metadata.created_local_date.empty() ? "Details" : metadata.created_local_date;
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

}  // namespace

DetailsPageCoordinator::DetailsPageCoordinator()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

void DetailsPageCoordinator::QueueShow(const std::string& recording_id,
                                       DetailsPageSource source_page)
{
    pending_recording_id_ = recording_id;
    pending_source_page_ = source_page;
}

void DetailsPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    scroll_container_active_ = false;
    ResetScrollPosition();
    title_text_ = "Details";
    recording_header_ = {};
    transcript_text_.clear();
    has_transcript_ = false;

    if (!pending_recording_id_.empty()) {
        recording_id_ = pending_recording_id_;
        source_page_ = pending_source_page_;
        pending_recording_id_.clear();
        pending_source_page_ = DetailsPageSource::kUnknown;
    }

    RefreshFromArchive(recordings);
    // Fresh page load: home focus on the scroll container. RefreshFromArchive() has already rebuilt
    // the navigation model to match whether this recording has a transcript.
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

void DetailsPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    const RecordingEntry* entry = FindEntry(recordings);
    if (entry == nullptr) {
        title_text_ = "Details";
        recording_header_ = {};
        transcript_text_.clear();
        has_transcript_ = false;
    } else {
        ApplyEntry(*entry);
    }
    UpdateNavigationModel();
}

void DetailsPageCoordinator::UpdateNavigationModel()
{
    // The primary action button is always present for a recording (Play once a
    // transcript exists, Transcribe otherwise), so it occupies the same control
    // slot in both states — only its label/intent change.
    const bool want_primary = true;
    const bool have_primary =
        navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageTranscribeButton) >= 0;
    if (want_primary == have_primary) {
        return;
    }
    navigation_model_ = page_navigation::BuildDetailsPageNavigationModel(want_primary);
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

bool DetailsPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    if (scroll_container_active_) {
        const int step = delta > 0 ? kScrollStepPercent : -kScrollStepPercent;
        const int next = std::clamp(scroll_position_percent_ + step, 0, 100);
        if (next == scroll_position_percent_) {
            return false;
        }
        scroll_position_percent_ = next;
        return true;
    }
    return focus_.Move(delta);
}

bool DetailsPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool DetailsPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

bool DetailsPageCoordinator::EnterScrollContainer()
{
    if (scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = true;
    return true;
}

bool DetailsPageCoordinator::ExitScrollContainer()
{
    if (!scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = false;
    return true;
}

epaper_ui::DetailsPageState DetailsPageCoordinator::BuildState() const
{
    epaper_ui::DetailsPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.title_text = title_text_;
    state.recording_header = recording_header_;
    state.scroll_container.content_text = has_transcript_ ? transcript_text_ : std::string();
    state.scroll_container.empty_state_message =
        has_transcript_ ? std::string() : kNoTranscriptMessage;
    state.scroll_container.focused =
        IsRoleFocused(NavigationItemRole::kDetailsPageScrollContainer) || scroll_container_active_;
    state.scroll_container.active = scroll_container_active_;
    state.scroll_container.scroll_position_percent = scroll_position_percent_;
    state.back_button.label_text = "Back";
    state.back_button.selected = IsRoleFocused(NavigationItemRole::kDetailsPageBackButton);
    // The primary action button sits beside Back: it plays the recording once a
    // transcript exists, and otherwise transcribes it. (The reused button/role is
    // still named "transcribe" in the layout; only the label and intent vary.)
    state.show_transcribe_button = true;
    state.transcribe_button.label_text = has_transcript_ ? "Play" : "Transcribe";
    state.transcribe_button.selected =
        IsRoleFocused(NavigationItemRole::kDetailsPageTranscribeButton);
    return state;
}

const RecordingEntry* DetailsPageCoordinator::FindEntry(
    const std::vector<RecordingEntry>& recordings) const
{
    if (recording_id_.empty()) {
        return nullptr;
    }
    for (const RecordingEntry& entry : recordings) {
        if (entry.recording_id == recording_id_) {
            return &entry;
        }
    }
    return nullptr;
}

void DetailsPageCoordinator::ApplyEntry(const RecordingEntry& entry)
{
    const std::string transcript = TrimTranscript(entry.transcript_text);
    has_transcript_ = entry.metadata.has_transcript && !transcript.empty();
    transcript_text_ = transcript;
    title_text_ = FormatDateLabel(entry.metadata);
    if (title_text_.empty()) {
        title_text_ = "Details";
    }

    recording_header_ = {};
    recording_header_.icon_asset = project_assets::GetIcon(
        has_transcript_ ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
    recording_header_.tag_icon_asset =
        entry.metadata.follow_up ? project_assets::GetIcon(EmbeddedIconId::kPin) : nullptr;
    recording_header_.time_text = FormatTimeLabel(entry);
    recording_header_.minute_seconds_text = FormatDurationLabel(entry.metadata.duration_ms);
    recording_header_.tag_text = TagText(entry.metadata.tag);
    recording_header_.selected = false;
}
