#include "settings_page_runtime.h"

#include <climits>
#include <mutex>

#include "esp_log.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "settings_page_interactions.h"
#include "settings_page_coordinator.h"
#include "storage_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"

namespace settings_page_runtime {
namespace {

constexpr const char* kTag = "SettingsPageRuntime";

std::mutex s_mutex;
SettingsPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;

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

page_navigation::NavigationItemRole RoleForUiItem(epaper_ui::SettingsPageItemId item)
{
    switch (item) {
        case epaper_ui::SettingsPageItemId::kWifiToggle:
            return page_navigation::NavigationItemRole::kSettingsWifiToggle;
        case epaper_ui::SettingsPageItemId::kAccessPointToggle:
            return page_navigation::NavigationItemRole::kSettingsEnableApToggle;
        case epaper_ui::SettingsPageItemId::kFormatSdButton:
            return page_navigation::NavigationItemRole::kSettingsFormatSdButton;
        case epaper_ui::SettingsPageItemId::kNone:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          s_coordinator.focus().index(),
                                          -1,
                                          -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          old_focus_index,
                                          -1,
                                          -1);
    const page_navigation::PageFocusProjection new_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          new_focus_index,
                                          -1,
                                          -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::SettingsPageState BuildStateLocked()
{
    return s_coordinator.BuildState(wifi_service::GetUiState(), storage_service::GetSnapshot());
}

bool ResolveTouchTargetImpl(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }

    const epaper_ui::SettingsPageState state = [&]() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return BuildStateLocked();
    }();

    epaper_ui::SettingsPageItemId item = epaper_ui::SettingsPageItemId::kNone;
    if (!epaper_ui::HitTestSettingsPageItem(display_service::PortraitWidth(),
                                            display_service::PortraitHeight(),
                                            state,
                                            x,
                                            y,
                                            &item)) {
        return false;
    }

    const page_navigation::NavigationItemRole role = RoleForUiItem(item);
    const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
    if (role == page_navigation::NavigationItemRole::kUnknown || focus_index < 0) {
        return false;
    }

    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kPage,
            .kind = app_interaction::Kind::kPageAction,
            .primary_index = focus_index,
            .secondary_index = s_interaction_generation,
        };
    }
    return true;
}

page_actions::FocusUpdateOutcome FocusTouchTargetImpl(
    const app_interaction::InteractiveTarget& target)
{
    page_actions::FocusUpdateOutcome result = {};
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return result;
    }

    bool changed = false;
    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::SettingsPageState old_state = {};
    epaper_ui::SettingsPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale settings focus target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        changed = s_coordinator.SetFocusIndex(target.primary_index);
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
    }

    if (!changed) {
        return result;
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
}

settings_page_interactions::ActivateResult ActivateTouchTargetImpl(
    const app_interaction::InteractiveTarget& target)
{
    settings_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale settings activate target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return result;
        }
        (void)s_coordinator.SetFocusIndex(target.primary_index);
        result = settings_page_interactions::HandlePrimaryActivate(s_coordinator);
    }
    return result;
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetSettingsPageState(BuildStateLocked());
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
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kSettingsPage, &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::SettingsPageState old_state = {};
    epaper_ui::SettingsPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        result = settings_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
}

settings_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return settings_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role =
        FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::SettingsPageState old_state = {};
    epaper_ui::SettingsPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show();
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    return ResolveTouchTargetImpl(x, y, target);
}

page_actions::FocusUpdateOutcome FocusTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    return FocusTouchTargetImpl(target);
}

settings_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    return ActivateTouchTargetImpl(target);
}

}  // namespace settings_page_runtime
