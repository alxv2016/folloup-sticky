#include "todos_page_runtime.h"

#include <climits>
#include <mutex>
#include <vector>

#include "epaper_ui/select_modal.h"
#include "epaper_ui/todos_page.h"
#include "esp_log.h"
#include "overlay_runtime.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "todos_page_coordinator.h"
#include "ui_refresh_runtime.h"

namespace todos_page_runtime {
namespace {

constexpr const char* kTag = "TodosPageRuntime";
constexpr int kItemIndexBits = 16;
constexpr int32_t kItemIndexMask = (1 << kItemIndexBits) - 1;

enum class ItemAction : uint8_t {
    kViewDetails,
    kFollowUp,
    kComplete,
    kDelete,
    kClose,
};

// Value snapshot of the selected todo, captured under the lock so the modal dispatch can use it
// without re-entering the coordinator.
struct SelectedEntrySnapshot {
    bool valid = false;
    std::string recording_id = {};
    std::string recording_path = {};
    bool follow_up = false;
    bool follow_up_completed = false;
    bool completed = false;
};

std::mutex s_mutex;
TodosPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;

bool s_item_actions_pending = false;
SelectedEntrySnapshot s_item_actions_entry = {};
std::vector<ItemAction> s_item_actions = {};
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

epaper_ui::TodosPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTodosPageTimelineGroups,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTodosPageTimelineGroups, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kTodosPageTimelineGroups, new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetTodosPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTodosPage,
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
        result = todos_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

todos_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return todos_page_interactions::HandlePrimaryActivate(s_coordinator);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    epaper_ui::TodosPageState state;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state = BuildStateLocked();
        generation = s_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();
    const epaper_ui::TodosTimelineHit hit =
        epaper_ui::HitTestTodosTimeline(portrait_width, portrait_height, state, x, y);

    if (hit.kind == epaper_ui::NotesTimelineHitKind::kGroupChip) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageListRow,  // date chip (primary = group index)
                .primary_index = hit.group_index,
                .secondary_index = generation,
            };
        }
        return true;
    }
    if (hit.kind == epaper_ui::NotesTimelineHitKind::kItem) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageComposite,  // item row (packed group<<16|item)
                .primary_index = (hit.group_index << kItemIndexBits) | (hit.item_index & kItemIndexMask),
                .secondary_index = generation,
            };
        }
        return true;
    }
    return false;
}

page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    page_actions::FocusUpdateOutcome result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        if (target.kind == app_interaction::Kind::kPageListRow) {
            if (!s_coordinator.FocusGroupChip(target.primary_index)) {
                return result;
            }
        } else if (target.kind == app_interaction::Kind::kPageComposite) {
            const int group_index = target.primary_index >> kItemIndexBits;
            const int item_index = target.primary_index & kItemIndexMask;
            if (!s_coordinator.EnterGroupItem(group_index, item_index)) {
                return result;
            }
        } else {
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

todos_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    todos_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (target.secondary_index != s_interaction_generation) {
        return result;
    }
    // Touch-down already focused the chip / entered the item (FocusTouchTarget); the release runs
    // the normal primary activation (enter the group, or open the item-actions modal).
    return todos_page_interactions::HandlePrimaryActivate(s_coordinator);
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
        ESP_LOGW(kTag, "Todos sync failed: %s", esp_err_to_name(err));
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

void SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.SetEntryFollowUpState(recording_id, follow_up, follow_up_completed);
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void SetEntryChecked(const std::string& recording_id, bool checked)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.SetEntryChecked(recording_id, checked);
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

bool ShowItemActionsModal()
{
    epaper_ui::SelectModalState modal = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const TodosPageCoordinator::TimelineEntry* entry = s_coordinator.SelectedEntry();
        if (entry == nullptr) {
            return false;
        }
        s_item_actions_entry = {
            .valid = true,
            .recording_id = entry->recording_id,
            .recording_path = entry->recording_path,
            .follow_up = entry->follow_up,
            .follow_up_completed = entry->follow_up_completed,
            .completed = entry->completed,
        };
        s_item_actions.clear();
        modal.title_text = "Todo";
        modal.items.push_back({"View details"});
        s_item_actions.push_back(ItemAction::kViewDetails);
        modal.items.push_back({entry->follow_up ? "Remove follow-up" : "Follow up"});
        s_item_actions.push_back(ItemAction::kFollowUp);
        modal.items.push_back({entry->completed ? "Mark incomplete" : "Complete"});
        s_item_actions.push_back(ItemAction::kComplete);
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
        case ItemAction::kViewDetails: {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_pending_view_details_id = entry.recording_id;
            break;
        }
        case ItemAction::kFollowUp: {
            const bool next_follow_up = !entry.follow_up;
            SetEntryFollowUpState(entry.recording_id, next_follow_up, false);
            if (!recording_archive_service::MarkRecordingFollowUp(entry.recording_id,
                                                                  next_follow_up, false)) {
                SetEntryFollowUpState(entry.recording_id, entry.follow_up,
                                      entry.follow_up_completed);
            }
            break;
        }
        case ItemAction::kComplete: {
            const bool next_completed = !entry.completed;
            // Optimistic checkbox toggle, then persist; revert on failure. MarkRecordingCompleted
            // re-aggregates the archive counts so the dashboard reflects the change.
            SetEntryChecked(entry.recording_id, next_completed);
            if (!recording_archive_service::MarkRecordingCompleted(entry.recording_id,
                                                                   next_completed)) {
                SetEntryChecked(entry.recording_id, entry.completed);
            }
            break;
        }
        case ItemAction::kDelete:
            (void)recording_archive_service::DeleteRecording(entry.recording_id);
            (void)SyncFromArchive(true);
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

}  // namespace todos_page_runtime
