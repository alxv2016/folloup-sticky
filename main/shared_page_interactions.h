#ifndef SHARED_PAGE_INTERACTIONS_H_
#define SHARED_PAGE_INTERACTIONS_H_

#include "page_action_result.h"
#include "page_navigation/navigation_model.h"

namespace shared_page_interactions {

template <typename ActivateResult, typename ActivateIntent, typename Coordinator>
ActivateResult HandleFooterPrimaryActivate(const Coordinator& coordinator,
                                          ActivateIntent show_home_intent,
                                          ActivateIntent show_settings_intent,
                                          ActivateIntent footer_wifi_intent,
                                          ActivateIntent footer_time_intent)
{
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterHome)) {
        return {
            .intent = show_home_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterSettings)) {
        return {
            .intent = show_settings_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterWifi)) {
        return {
            .intent = footer_wifi_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kFooterTime)) {
        return {
            .intent = footer_time_intent,
            .handled = true,
            .play_activate_cue = true,
        };
    }

    return {};
}

template <typename Coordinator>
page_actions::FocusMoveOutcome HandleMoveFocus(Coordinator& coordinator, int delta)
{
    if (delta == 0 || !coordinator.MoveFocus(delta)) {
        return {};
    }

    page_actions::FocusMoveOutcome outcome = {};
    outcome.handled = true;
    outcome.play_navigation_cue = true;
    outcome.apply_page_state = true;
    return outcome;
}

}  // namespace shared_page_interactions

#endif  // SHARED_PAGE_INTERACTIONS_H_
