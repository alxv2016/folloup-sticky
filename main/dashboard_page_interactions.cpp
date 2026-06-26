#include "dashboard_page_interactions.h"

namespace dashboard_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(DashboardPageCoordinator& coordinator)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;

    const int menu_index = coordinator.FocusedMenuIndex();
    if (menu_index >= 0) {
        result.intent = ActivateIntent::kOpenMenuItem;
        result.menu_index = menu_index;
        return result;
    }

    switch (coordinator.FocusedRole()) {
        case NavigationItemRole::kFooterHome:
            result.intent = ActivateIntent::kShowHome;
            break;
        case NavigationItemRole::kFooterSettings:
            result.intent = ActivateIntent::kShowSettings;
            break;
        case NavigationItemRole::kFooterWifi:
            result.intent = ActivateIntent::kShowWifi;
            break;
        case NavigationItemRole::kFooterTime:
            result.intent = ActivateIntent::kShowTime;
            break;
        default:
            result.handled = false;
            result.play_activate_cue = false;
            break;
    }
    return result;
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    switch (result.intent) {
        case ActivateIntent::kOpenMenuItem:
            if (callbacks.open_menu_item) {
                callbacks.open_menu_item(result.menu_index);
            }
            break;
        case ActivateIntent::kShowHome:
            if (callbacks.show_home) {
                callbacks.show_home();
            }
            break;
        case ActivateIntent::kShowSettings:
            if (callbacks.show_settings) {
                callbacks.show_settings();
            }
            break;
        case ActivateIntent::kShowWifi:
            if (callbacks.show_wifi) {
                callbacks.show_wifi();
            }
            break;
        case ActivateIntent::kShowTime:
            if (callbacks.show_time) {
                callbacks.show_time();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(DashboardPageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    result.sync_footer_projection = true;
    return result;
}

}  // namespace dashboard_page_interactions
