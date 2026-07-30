#include "notes_page_runtime.h"

#include <climits>
#include <mutex>
#include <vector>

#include "epaper_ui/notes_page.h"
#include "epaper_ui/select_modal.h"
#include "esp_log.h"
#include "notes_page_coordinator.h"
#include "clip_playback_runtime.h"
#include "overlay_runtime.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "ui_refresh_runtime.h"

namespace notes_page_runtime {
namespace {

constexpr const char* kTag = "NotesPageRuntime";
constexpr int kItemIndexBits = 16;
constexpr int32_t kItemIndexMask = (1 << kItemIndexBits) - 1;

enum class ItemAction : uint8_t {
    kPlayRecording,
    kViewDetails,
    kFollowUp,
    kTurnToTask,
    kDelete,
    kClose,
};

std::mutex s_mutex;
NotesPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;

// The item-actions modal is a shared select_modal; track the selection so the app_shell submit
// chain can route it back here, and remember the row->action mapping shown to the user.
bool s_item_actions_pending = false;
SelectedEntrySnapshot s_item_actions_entry = {};
std::vector<ItemAction> s_item_actions = {};
// View details opens another screen, so (like Back) it's deferred: the selection stashes the id
// and app_shell polls ConsumePendingViewDetails() after the submit chain.
std::string s_pending_view_details_id = {};

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
    } else {
        ++s_interaction_generation;
    }
}

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

epaper_ui::NotesPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kNotesPageTimelineGroups,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kNotesPageTimelineGroups, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kNotesPageTimelineGroups, new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetNotesPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(display_service::RefreshRequest{
        .refresh_mode = refresh_mode,
    });
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kNotesPage,
                                        &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        result = notes_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

notes_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return notes_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role = FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        s_coordinator.ExitItemList();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t SyncFromArchive(bool request_refresh_if_active)
{
    std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RefreshFromArchive(entries);
    }
    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Notes sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool ExitActiveControl()
{
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        exited = s_coordinator.ExitItemList();
    }
    if (exited) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return exited;
}

SelectedEntrySnapshot GetSelectedEntrySnapshot()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    SelectedEntrySnapshot snapshot = {};
    const NotesPageCoordinator::TimelineEntry* entry = s_coordinator.SelectedEntry();
    if (entry != nullptr) {
        snapshot.valid = true;
        snapshot.recording_id = entry->recording_id;
        snapshot.recording_path = entry->recording_path;
        snapshot.follow_up = entry->follow_up;
        snapshot.follow_up_completed = entry->follow_up_completed;
    }
    return snapshot;
}

void SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.SetEntryFollowUpState(recording_id, follow_up, follow_up_completed);
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

bool ShowItemActionsModal()
{
    epaper_ui::SelectModalState modal = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const NotesPageCoordinator::TimelineEntry* entry = s_coordinator.SelectedEntry();
        if (entry == nullptr) {
            return false;
        }
        s_item_actions_entry = {
            .valid = true,
            .recording_id = entry->recording_id,
            .recording_path = entry->recording_path,
            .follow_up = entry->follow_up,
            .follow_up_completed = entry->follow_up_completed,
        };
        s_item_actions.clear();
        modal.title_text = "Note";
        // Only offered when the row actually has audio on the card; a transcript-only
        // entry would otherwise show an action that silently does nothing.
        if (!entry->recording_path.empty()) {
            modal.items.push_back({"Play recording"});
            s_item_actions.push_back(ItemAction::kPlayRecording);
        }
        modal.items.push_back({"View details"});
        s_item_actions.push_back(ItemAction::kViewDetails);
        modal.items.push_back({entry->follow_up ? "Remove follow-up" : "Follow up"});
        s_item_actions.push_back(ItemAction::kFollowUp);
        modal.items.push_back({"Turn to task"});
        s_item_actions.push_back(ItemAction::kTurnToTask);
        modal.items.push_back({"Delete"});
        s_item_actions.push_back(ItemAction::kDelete);
        modal.items.push_back({"Close"});
        s_item_actions.push_back(ItemAction::kClose);
        modal.selected_index = 0;
        s_item_actions_pending = true;
    }
    const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_item_actions_pending = false;
        ESP_LOGW(kTag, "Show item-actions modal failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool HandleItemActionSelection(int selected_index)
{
    ItemAction action = ItemAction::kClose;
    SelectedEntrySnapshot entry = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_item_actions_pending) {
            return false;
        }
        s_item_actions_pending = false;
        entry = s_item_actions_entry;
        if (selected_index < 0 || selected_index >= static_cast<int>(s_item_actions.size())) {
            return true;  // dismissed without a valid selection
        }
        action = s_item_actions[static_cast<size_t>(selected_index)];
    }
    if (!entry.valid) {
        return true;
    }

    switch (action) {
        case ItemAction::kPlayRecording:
            // Streams on a worker; the modal closes immediately rather than blocking the
            // submit chain for the length of the clip.
            (void)clip_playback_runtime::PlayFileAsync(entry.recording_path);
            break;
        case ItemAction::kViewDetails: {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_pending_view_details_id = entry.recording_id;
            break;
        }
        case ItemAction::kFollowUp: {
            const bool next_follow_up = !entry.follow_up;
            // Optimistic repaint, then persist; revert on failure.
            SetEntryFollowUpState(entry.recording_id, next_follow_up, false);
            if (!recording_archive_service::MarkRecordingFollowUp(entry.recording_id,
                                                                  next_follow_up, false)) {
                SetEntryFollowUpState(entry.recording_id, entry.follow_up,
                                      entry.follow_up_completed);
            }
            break;
        }
        // No explicit SyncFromArchive here (or below): the archive mutators notify
        // subscribers on success, and app_shell's archive handler re-syncs whichever page
        // is on screen -- which is this one, since the action modal opened over it. Syncing
        // again would repeat a full ListRecordings (every entry plus every transcript file,
        // off the SPI bus the panel shares) and repaint identical data twice.
        case ItemAction::kTurnToTask:
            (void)recording_archive_service::UpdateRecordingTag(
                entry.recording_id, recording_archive_service::RecordingTag::kTask);
            break;
        case ItemAction::kDelete:
            (void)recording_archive_service::DeleteRecording(entry.recording_id);
            break;
        case ItemAction::kClose:
        default:
            break;
    }
    return true;
}

std::string ConsumePendingViewDetails()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    std::string id;
    id.swap(s_pending_view_details_id);
    return id;
}

}  // namespace notes_page_runtime
