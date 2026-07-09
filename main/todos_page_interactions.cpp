#include "todos_page_interactions.h"

namespace todos_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(TodosPageCoordinator& coordinator)
{
    ActivateResult result = {};

    if (coordinator.IsRoleFocused(NavigationItemRole::kTodosPageTimelineGroup)) {
        result.handled = true;
        if (coordinator.item_list_active()) {
            // A todo is selected -> open its actions modal.
            result.intent = ActivateIntent::kOpenItemActions;
            result.play_activate_cue = true;
        } else if (coordinator.EnterFocusedGroup()) {
            // Entered the group's item list; repaint to show the active state.
            result.play_activate_cue = true;
            result.apply_page_state = true;
        }
        return result;
    }

    result.handled = true;
    result.play_activate_cue = true;
    if (coordinator.IsRoleFocused(NavigationItemRole::kFooterHome)) {
        result.intent = ActivateIntent::kShowHome;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kFooterSettings)) {
        result.intent = ActivateIntent::kShowSettings;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kFooterWifi)) {
        result.intent = ActivateIntent::kShowWifi;
    } else if (coordinator.IsRoleFocused(NavigationItemRole::kFooterTime)) {
        result.intent = ActivateIntent::kShowTime;
    } else {
        result.handled = false;
        result.play_activate_cue = false;
    }
    return result;
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    switch (result.intent) {
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
        case ActivateIntent::kOpenItemActions:
            if (callbacks.open_item_actions) {
                callbacks.open_item_actions();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(TodosPageCoordinator& coordinator, int delta)
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

}  // namespace todos_page_interactions
