#include "wifi_page_interactions.h"

#include "shared_page_interactions.h"

namespace wifi_page_interactions {

ActivateResult HandlePrimaryActivate(WifiPageCoordinator& coordinator)
{
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageNetworkList)) {
        ActivateResult result = {
            .handled = true,
            .play_activate_cue = true,
        };
        if (coordinator.network_list_active()) {
            (void)coordinator.CommitFocusedNetworkSelection();
            (void)coordinator.ExitNetworkList();
            result.apply_page_state = true;
            return result;
        }
        if (coordinator.HasNetworks() && coordinator.EnterNetworkList()) {
            result.apply_page_state = true;
        }
        return result;
    }

    const ActivateResult footer_result =
        shared_page_interactions::HandleFooterPrimaryActivate<ActivateResult>(
            coordinator,
            ActivateIntent::kShowHome,
            ActivateIntent::kShowSettings,
            ActivateIntent::kForceRefresh);
    if (footer_result.handled) {
        return footer_result;
    }

    if (coordinator.IsRoleFocused(
            page_navigation::NavigationItemRole::kWifiPagePasswordInput)) {
        return {
            .intent = ActivateIntent::kOpenPasswordKeyboard,
            .handled = true,
            .play_activate_cue = true,
            .apply_page_state = true,
        };
    }
    if (coordinator.IsRoleFocused(
            page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton)) {
        (void)coordinator.TogglePasswordVisibility();
        return {
            .handled = true,
            .play_activate_cue = true,
            .apply_page_state = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageScanButton)) {
        return {
            .intent = ActivateIntent::kStartNetworkScan,
            .handled = true,
            .play_activate_cue = true,
        };
    }
    if (coordinator.IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageConnectButton)) {
        return {
            .intent = ActivateIntent::kToggleSelectedNetworkConnection,
            .handled = true,
            .play_activate_cue = true,
            .disconnect_current_network = coordinator.SelectedNetworkIsCurrent(),
            .connect_ssid = coordinator.SelectedNetworkSsid(),
            .connect_password = coordinator.password_input_state().value_text,
        };
    }

    return {};
}

void ApplyPrimaryActivateResult(const ActivateResult& result,
                                const ActivateCallbacks& callbacks)
{
    if (!result.handled) {
        return;
    }

    switch (result.intent) {
        case ActivateIntent::kShowHome:
            if (callbacks.show_home) {
                callbacks.show_home();
            }
            return;
        case ActivateIntent::kShowSettings:
            if (callbacks.show_settings) {
                callbacks.show_settings();
            }
            return;
        case ActivateIntent::kForceRefresh:
            if (callbacks.force_refresh) {
                callbacks.force_refresh();
            }
            return;
        case ActivateIntent::kOpenPasswordKeyboard:
            if (callbacks.open_password_keyboard) {
                callbacks.open_password_keyboard();
            }
            return;
        case ActivateIntent::kStartNetworkScan:
            if (callbacks.start_network_scan) {
                callbacks.start_network_scan();
            }
            return;
        case ActivateIntent::kToggleSelectedNetworkConnection:
            if (callbacks.toggle_selected_network_connection) {
                callbacks.toggle_selected_network_connection(
                    result.disconnect_current_network, result.connect_ssid,
                    result.connect_password);
            }
            return;
        case ActivateIntent::kNone:
        default:
            return;
    }
}

FocusMoveResult HandleMoveFocus(WifiPageCoordinator& coordinator,
                                int delta,
                                bool page_jump,
                                int page_size)
{
    const bool moved =
        page_jump ? coordinator.MoveFocusByPage(delta, page_size) : coordinator.MoveFocus(delta);
    if (delta == 0 || !moved) {
        return {};
    }

    page_actions::FocusMoveOutcome outcome = {};
    outcome.handled = true;
    outcome.play_navigation_cue = true;
    outcome.apply_page_state = true;
    return outcome;
}

ActivateResult HandleNetworkRowActivate(WifiPageCoordinator& coordinator, int row_index)
{
    if (!coordinator.FocusNetworkRow(row_index) ||
        !coordinator.SelectNetworkRow(row_index)) {
        return {};
    }

    return {
        .handled = true,
        .play_activate_cue = true,
        .apply_page_state = true,
    };
}

SecondaryActivateResult HandleSecondaryActivate(WifiPageCoordinator& coordinator)
{
    if (!coordinator.network_list_active() || !coordinator.ExitNetworkList()) {
        return {};
    }

    return {
        .intent = SecondaryActivateIntent::kExitNetworkList,
        .handled = true,
        .play_activate_cue = true,
        .apply_page_state = true,
    };
}

void ApplySecondaryActivateResult(const SecondaryActivateResult& result,
                                  const SecondaryActivateCallbacks& callbacks)
{
    if (!result.handled) {
        return;
    }

    if (result.play_activate_cue && callbacks.play_activate_cue) {
        callbacks.play_activate_cue();
    }
    if (result.apply_page_state && callbacks.apply_page_state) {
        callbacks.apply_page_state();
    }
}

}  // namespace wifi_page_interactions
