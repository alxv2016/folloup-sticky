#include "dashboard_page_runtime.h"

#include <climits>
#include <mutex>

#include "dashboard_page_coordinator.h"
#include "epaper_ui/dashboard_page.h"
#include "esp_log.h"
#include "overlay_runtime.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_archive_service.h"
#include "ui_refresh_runtime.h"

namespace dashboard_page_runtime {
namespace {

constexpr const char* kTag = "DashboardPageRuntime";

std::mutex s_mutex;
DashboardPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
uint32_t s_last_welcome_period = 0;
bool s_welcome_period_valid = false;
MenuItemHandler s_menu_item_handler = nullptr;
void* s_menu_item_context = nullptr;

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

epaper_ui::DashboardPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kDashboardPageMenu,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kDashboardPageMenu,
        old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kDashboardPageMenu,
        new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetDashboardPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kDashboardPage,
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
        result = dashboard_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

dashboard_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return dashboard_page_interactions::HandlePrimaryActivate(s_coordinator);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    const epaper_ui::DashboardPageState state = [&]() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return BuildStateLocked();
    }();

    int menu_index = -1;
    if (!epaper_ui::HitTestDashboardMenuItem(display_service::PortraitWidth(),
                                             display_service::PortraitHeight(), state, x, y,
                                             &menu_index)) {
        return false;
    }
    // Menu items occupy focus indices 0..n-1, so the menu index is the focus index.
    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kPage,
            .kind = app_interaction::Kind::kPageAction,
            .primary_index = menu_index,
            .secondary_index = s_interaction_generation,
        };
    }
    return true;
}

page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    page_actions::FocusUpdateOutcome result = {};
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return result;
    }
    bool changed = false;
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        changed = s_coordinator.SetFocusIndex(target.primary_index);
        new_focus_index = s_coordinator.focus().index();
    }
    if (!changed) {
        return result;
    }
    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

dashboard_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    dashboard_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (target.secondary_index != s_interaction_generation) {
        return result;
    }
    (void)s_coordinator.SetFocusIndex(target.primary_index);
    return dashboard_page_interactions::HandlePrimaryActivate(s_coordinator);
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
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RefreshFromArchive(recording_archive_service::GetSnapshot());
        // Re-baseline the rotation so RefreshWelcomeIfRotated only fires on later rollovers.
        s_last_welcome_period = DashboardPageCoordinator::WelcomePeriodsSinceEpoch();
        s_welcome_period_valid = true;
    }
    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Dashboard sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t RefreshWelcomeIfRotated()
{
    const uint32_t period = DashboardPageCoordinator::WelcomePeriodsSinceEpoch();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_welcome_period_valid && period == s_last_welcome_period) {
            return ESP_OK;  // still in the same interval; nothing to repaint
        }
        s_last_welcome_period = period;
        s_welcome_period_valid = true;
    }
    return UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

void SetMenuItemHandler(MenuItemHandler handler, void* context)
{
    s_menu_item_handler = handler;
    s_menu_item_context = context;
}

void OpenMenuItem(int menu_index)
{
    if (s_menu_item_handler != nullptr && s_menu_item_handler(menu_index, s_menu_item_context)) {
        return;
    }
    ESP_LOGI(kTag, "Menu item %d (%s) not yet implemented", menu_index,
             epaper_ui::DashboardMenuItemLabel(menu_index));
    epaper_ui::ToastState toast = {};
    toast.visible = true;
    toast.body_text = "Coming soon";
    (void)overlay_runtime::ShowToastForDuration(toast, 1500);
}

}  // namespace dashboard_page_runtime
