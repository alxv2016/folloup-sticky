#include "time_page_interactions.h"

namespace time_page_interactions {
namespace {

using page_navigation::NavigationItemRole;

}  // namespace

ActivateResult HandlePrimaryActivate(TimePageCoordinator& coordinator)
{
    ActivateResult result = {};
    result.handled = true;
    result.play_activate_cue = true;
    switch (coordinator.FocusedRole()) {
        case NavigationItemRole::kTimePageTimezone:
            result.intent = ActivateIntent::kShowTimezoneModal;
            break;
        case NavigationItemRole::kTimePageHour:
            result.intent = ActivateIntent::kEditHour;
            break;
        case NavigationItemRole::kTimePageMinute:
            result.intent = ActivateIntent::kEditMinute;
            break;
        case NavigationItemRole::kTimePageMeridiem:
            result.intent = ActivateIntent::kToggleMeridiem;
            result.apply_page_state = true;
            break;
        case NavigationItemRole::kTimePageMonth:
            result.intent = ActivateIntent::kEditMonth;
            break;
        case NavigationItemRole::kTimePageDay:
            result.intent = ActivateIntent::kEditDay;
            break;
        case NavigationItemRole::kTimePageYear:
            result.intent = ActivateIntent::kEditYear;
            break;
        case NavigationItemRole::kTimePageSave:
            result.intent = ActivateIntent::kSave;
            break;
        case NavigationItemRole::kFooterHome:
            result.intent = ActivateIntent::kShowHome;
            break;
        case NavigationItemRole::kFooterSettings:
            result.intent = ActivateIntent::kShowSettings;
            break;
        case NavigationItemRole::kFooterWifi:
            result.intent = ActivateIntent::kShowWifi;
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
        case ActivateIntent::kShowTimezoneModal:
            if (callbacks.show_timezone_modal) {
                callbacks.show_timezone_modal();
            }
            break;
        case ActivateIntent::kEditHour:
            if (callbacks.edit_hour) {
                callbacks.edit_hour();
            }
            break;
        case ActivateIntent::kEditMinute:
            if (callbacks.edit_minute) {
                callbacks.edit_minute();
            }
            break;
        case ActivateIntent::kToggleMeridiem:
            if (callbacks.toggle_meridiem) {
                callbacks.toggle_meridiem();
            }
            break;
        case ActivateIntent::kEditMonth:
            if (callbacks.edit_month) {
                callbacks.edit_month();
            }
            break;
        case ActivateIntent::kEditDay:
            if (callbacks.edit_day) {
                callbacks.edit_day();
            }
            break;
        case ActivateIntent::kEditYear:
            if (callbacks.edit_year) {
                callbacks.edit_year();
            }
            break;
        case ActivateIntent::kSave:
            if (callbacks.save) {
                callbacks.save();
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
        case ActivateIntent::kNone:
        default:
            break;
    }
}

FocusMoveResult HandleMoveFocus(TimePageCoordinator& coordinator, int delta)
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

}  // namespace time_page_interactions
