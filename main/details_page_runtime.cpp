#include "details_page_runtime.h"

#include <atomic>
#include <climits>
#include <mutex>

#include "epaper_ui/details_page.h"
#include "esp_log.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "ui_refresh_runtime.h"

namespace details_page_runtime {
namespace {

constexpr const char* kTag = "DetailsPageRuntime";

std::mutex s_mutex;
DetailsPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
std::atomic<bool> s_pending_back{false};

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
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

epaper_ui::DetailsPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kDetailsPageControls,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kDetailsPageControls, old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(),
        page_navigation::NavigationItemSection::kDetailsPageControls, new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetDetailsPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kDetailsPage,
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
        result = details_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

details_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return details_page_interactions::HandlePrimaryActivate(s_coordinator);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    epaper_ui::DetailsPageState state;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state = BuildStateLocked();
        generation = s_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();

    if (epaper_ui::HitTestDetailsBackButton(portrait_width, portrait_height, state, x, y)) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageAction,  // Back button
                .primary_index = 0,
                .secondary_index = generation,
            };
        }
        return true;
    }
    if (epaper_ui::HitTestDetailsScrollContainer(portrait_width, portrait_height, state, x, y)) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageComposite,  // scroll container
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
        page_navigation::NavigationItemRole role = page_navigation::NavigationItemRole::kUnknown;
        if (target.kind == app_interaction::Kind::kPageComposite) {
            role = page_navigation::NavigationItemRole::kDetailsPageScrollContainer;
        } else if (target.kind == app_interaction::Kind::kPageAction) {
            role = page_navigation::NavigationItemRole::kDetailsPageBackButton;
            s_coordinator.ExitScrollContainer();
        } else {
            return result;
        }
        s_coordinator.SetFocusIndex(s_coordinator.navigation_model().IndexOfRole(role));
        new_focus_index = s_coordinator.focus().index();
    }
    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

details_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    details_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (target.secondary_index != s_interaction_generation) {
        return result;
    }
    page_navigation::NavigationItemRole role = page_navigation::NavigationItemRole::kUnknown;
    if (target.kind == app_interaction::Kind::kPageComposite) {
        role = page_navigation::NavigationItemRole::kDetailsPageScrollContainer;
    } else if (target.kind == app_interaction::Kind::kPageAction) {
        role = page_navigation::NavigationItemRole::kDetailsPageBackButton;
    } else {
        return result;
    }
    s_coordinator.SetFocusIndex(s_coordinator.navigation_model().IndexOfRole(role));
    return details_page_interactions::HandlePrimaryActivate(s_coordinator);
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
        s_coordinator.ExitScrollContainer();
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

void QueueShow(const std::string& recording_id, DetailsPageSource source_page)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_coordinator.QueueShow(recording_id, source_page);
}

esp_err_t SyncFromArchive(bool request_refresh_if_active)
{
    std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show(entries);
    }
    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Details sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

DetailsPageSource SourcePage()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_coordinator.source_page();
}

void RequestBack()
{
    s_pending_back.store(true, std::memory_order_relaxed);
}

bool ConsumePendingBack()
{
    return s_pending_back.exchange(false, std::memory_order_relaxed);
}

bool ExitActiveControl()
{
    bool exited = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        exited = s_coordinator.ExitScrollContainer();
    }
    if (exited) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return exited;
}

}  // namespace details_page_runtime
