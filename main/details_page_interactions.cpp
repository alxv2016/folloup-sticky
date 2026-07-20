#include "details_page_interactions.h"

namespace details_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(DetailsPageCoordinator& coordinator)
{
    ActivateResult result = {};

    if (coordinator.IsRoleFocused(NavigationItemRole::kDetailsPageScrollContainer)) {
        result.handled = true;
        if (coordinator.scroll_container_active()) {
            // Already scrolling; OK is inert (DOWN double-click exits).
            return result;
        }
        coordinator.EnterScrollContainer();
        result.play_activate_cue = true;
        result.apply_page_state = true;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kDetailsPageBackButton)) {
        result.intent = ActivateIntent::kShowPreviousPage;
        result.handled = true;
        result.play_activate_cue = true;
        return result;
    }

    if (coordinator.IsRoleFocused(NavigationItemRole::kDetailsPageTranscribeButton)) {
        result.intent = ActivateIntent::kTranscribe;
        result.handled = true;
        result.play_activate_cue = true;
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
        case ActivateIntent::kShowPreviousPage:
            if (callbacks.show_previous_page) {
                callbacks.show_previous_page();
            }
            break;
        case ActivateIntent::kTranscribe:
            if (callbacks.transcribe) {
                callbacks.transcribe();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(DetailsPageCoordinator& coordinator, int delta)
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

}  // namespace details_page_interactions
