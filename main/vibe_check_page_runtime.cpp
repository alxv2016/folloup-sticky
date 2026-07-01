#include "vibe_check_page_runtime.h"

#include <climits>
#include <mutex>
#include <string>
#include <vector>

#include "epaper_ui/vibe_check_page.h"
#include "esp_log.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "ui_refresh_runtime.h"
#include "vibe_check_page_coordinator.h"

namespace vibe_check_page_runtime {
namespace {

constexpr const char* kTag = "VibeCheckPageRuntime";

std::mutex s_mutex;
VibeCheckPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
    } else {
        ++s_interaction_generation;
    }
}

int CardFocusIndexLocked()
{
    return s_coordinator.navigation_model().IndexOfRole(
        page_navigation::NavigationItemRole::kVibeCheckPageCard);
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
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

epaper_ui::VibeCheckPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kVibeCheckPageControls,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kVibeCheckPageControls, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kVibeCheckPageControls, new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetVibeCheckPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kVibeCheckPage,
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
        result = vibe_check_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

vibe_check_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return vibe_check_page_interactions::HandlePrimaryActivate(s_coordinator);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    epaper_ui::VibeCheckPageState state;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state = BuildStateLocked();
        generation = s_interaction_generation;
    }

    epaper_ui::VibeCardActionSelection action = epaper_ui::VibeCardActionSelection::kNone;
    if (epaper_ui::HitTestVibeCheckAction(display_service::PortraitWidth(),
                                          display_service::PortraitHeight(), state, x, y, &action)) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageAction,
                .primary_index = static_cast<int32_t>(action),
                .secondary_index = generation,
            };
        }
        return true;
    }

    if (epaper_ui::HitTestVibeCheckCard(display_service::PortraitWidth(),
                                        display_service::PortraitHeight(), state, x, y)) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageComposite,
                .primary_index = 0,
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
        s_coordinator.SetFocusIndex(CardFocusIndexLocked());
        if (target.kind == app_interaction::Kind::kPageAction) {
            s_coordinator.EnterCardAtAction(static_cast<int>(target.primary_index));
        } else if (target.kind == app_interaction::Kind::kPageComposite) {
            s_coordinator.ExitCard();
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

vibe_check_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    vibe_check_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (target.secondary_index != s_interaction_generation) {
        return result;
    }
    s_coordinator.SetFocusIndex(CardFocusIndexLocked());
    if (target.kind == app_interaction::Kind::kPageAction) {
        s_coordinator.EnterCardAtAction(static_cast<int>(target.primary_index));
        return vibe_check_page_interactions::HandlePrimaryActivate(s_coordinator);
    }
    if (target.kind == app_interaction::Kind::kPageComposite) {
        // A tap on the card body just focuses it; the action buttons are tapped directly.
        result.handled = true;
    }
    return result;
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
        s_coordinator.ExitCard();
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
        s_coordinator.PrepareForShow();
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t SyncFromService(bool request_refresh_if_active)
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
        ESP_LOGW(kTag, "Vibe check sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool ExitFocusedCard()
{
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        exited = s_coordinator.ExitCard();
    }
    if (exited) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return exited;
}

void EnterFocusedCard()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.EnterCard();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void RefreshIdea()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RandomizeIdea();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void DeleteCurrentIdea()
{
    std::string recording_id;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        recording_id = s_coordinator.current_recording_id();
    }
    if (recording_id.empty()) {
        return;
    }
    if (!recording_archive_service::DeleteRecording(recording_id)) {
        ESP_LOGW(kTag, "Delete idea failed: id=%s", recording_id.c_str());
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RemoveCurrentIdea();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void PinCurrentIdea()
{
    std::string recording_id;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        recording_id = s_coordinator.current_recording_id();
    }
    if (recording_id.empty()) {
        return;
    }
    if (!recording_archive_service::MarkRecordingFollowUp(recording_id, true, false)) {
        ESP_LOGW(kTag, "Pin idea failed: id=%s", recording_id.c_str());
    }
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RemoveCurrentIdea();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

}  // namespace vibe_check_page_runtime
