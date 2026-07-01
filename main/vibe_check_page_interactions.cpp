#include "vibe_check_page_interactions.h"

namespace vibe_check_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(VibeCheckPageCoordinator& coordinator)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;

    if (coordinator.IsRoleFocused(NavigationItemRole::kVibeCheckPageCard)) {
        if (coordinator.card_active()) {
            switch (coordinator.selected_action()) {
                case epaper_ui::VibeCardActionSelection::kRefresh:
                    result.intent = ActivateIntent::kRefreshIdea;
                    break;
                case epaper_ui::VibeCardActionSelection::kClose:
                    result.intent = ActivateIntent::kDeleteIdea;
                    break;
                case epaper_ui::VibeCardActionSelection::kCheck:
                    result.intent = ActivateIntent::kPinIdea;
                    break;
                case epaper_ui::VibeCardActionSelection::kNone:
                default:
                    break;
            }
            return result;
        }
        if (coordinator.HasIdeas()) {
            result.intent = ActivateIntent::kEnterCard;
            return result;
        }
        // Empty card: nothing to activate, but the press is still consumed by the page.
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
        case ActivateIntent::kEnterCard:
            if (callbacks.enter_card) {
                callbacks.enter_card();
            }
            break;
        case ActivateIntent::kRefreshIdea:
            if (callbacks.refresh_idea) {
                callbacks.refresh_idea();
            }
            break;
        case ActivateIntent::kDeleteIdea:
            if (callbacks.delete_idea) {
                callbacks.delete_idea();
            }
            break;
        case ActivateIntent::kPinIdea:
            if (callbacks.pin_idea) {
                callbacks.pin_idea();
            }
            break;
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(VibeCheckPageCoordinator& coordinator, int delta)
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

}  // namespace vibe_check_page_interactions
