#include "wifi_page_coordinator.h"

#include <algorithm>

namespace {

constexpr const char* kPasswordLabel = "Password";
constexpr const char* kPasswordPlaceholder = "Enter password";

int NextPageIndex(int current_index, int total_items, int items_per_page)
{
    if (total_items <= 0 || items_per_page <= 0) {
        return 0;
    }
    if (total_items <= items_per_page) {
        return page_navigation::RovingFocus::WrapIndex(current_index + 1, total_items);
    }

    const int last_page_index = (total_items - 1) / items_per_page;
    const int current_page_index = current_index / items_per_page;
    if (current_page_index < last_page_index) {
        return (current_page_index + 1) * items_per_page;
    }
    return 0;
}

int PreviousPageIndex(int current_index, int total_items, int items_per_page)
{
    if (total_items <= 0 || items_per_page <= 0) {
        return 0;
    }
    if (total_items <= items_per_page) {
        return page_navigation::RovingFocus::WrapIndex(current_index - 1, total_items);
    }

    const int last_page_index = (total_items - 1) / items_per_page;
    const int current_page_index = current_index / items_per_page;
    if (current_page_index > 0) {
        return (current_page_index - 1) * items_per_page;
    }
    return last_page_index * items_per_page;
}

}  // namespace

WifiPageCoordinator::WifiPageCoordinator()
{
    InitializePasswordInput();
}

void WifiPageCoordinator::RefreshFromService(const wifi_service::UiState& ui_state,
                                             const wifi_service::ScanSnapshot& scan_snapshot)
{
    if (scan_snapshot.state == wifi_service::ScanState::kRunning) {
        network_status_ = epaper_ui::NetworkListStatus::kIdle;
    } else if (!scan_snapshot.networks.empty()) {
        network_status_ = epaper_ui::NetworkListStatus::kNetworksFound;
    } else if (scan_snapshot.state == wifi_service::ScanState::kComplete ||
               scan_snapshot.state == wifi_service::ScanState::kFailed) {
        network_status_ = epaper_ui::NetworkListStatus::kNoNetworks;
    } else {
        network_status_ = epaper_ui::NetworkListStatus::kIdle;
    }

    std::string previously_selected_ssid = SelectedNetworkSsid();

    networks_.clear();
    networks_.reserve(scan_snapshot.networks.size() + 1);
    for (const wifi_service::ScannedNetwork& network : scan_snapshot.networks) {
        networks_.push_back({
            .ssid = network.ssid,
            .current_network = ui_state.connected && ui_state.ssid == network.ssid,
            .private_network = !network.IsOpen(),
            .signal_strength = network.rssi >= -60 ? epaper_ui::NetworkSignalStrength::kStrong
                                                   : epaper_ui::NetworkSignalStrength::kMedium,
        });
    }

    if (ui_state.connected && !ui_state.ssid.empty()) {
        const auto existing =
            std::find_if(networks_.begin(), networks_.end(), [&](const NetworkEntry& entry) {
                return entry.ssid == ui_state.ssid;
            });
        if (existing == networks_.end()) {
            networks_.push_back({
                .ssid = ui_state.ssid,
                .current_network = true,
                .private_network = true,
                .signal_strength = ui_state.rssi >= -60 ? epaper_ui::NetworkSignalStrength::kStrong
                                                        : epaper_ui::NetworkSignalStrength::kMedium,
            });
        }
    }

    int selected_index = epaper_ui::kNetworkListNoSelection;
    if (!previously_selected_ssid.empty()) {
        for (size_t index = 0; index < networks_.size(); ++index) {
            if (networks_[index].ssid == previously_selected_ssid) {
                selected_index = static_cast<int>(index);
                break;
            }
        }
    }
    if (selected_index == epaper_ui::kNetworkListNoSelection && ui_state.connected &&
        !ui_state.ssid.empty()) {
        for (size_t index = 0; index < networks_.size(); ++index) {
            if (networks_[index].ssid == ui_state.ssid) {
                selected_index = static_cast<int>(index);
                break;
            }
        }
    }
    if (selected_index == epaper_ui::kNetworkListNoSelection && !networks_.empty()) {
        selected_index = 0;
    }

    selected_network_index_ = selected_index;
    if (networks_.empty()) {
        network_list_active_ = false;
        network_focus_.Configure(0, 0);
    } else {
        const int initial_focus =
            selected_index != epaper_ui::kNetworkListNoSelection ? selected_index : 0;
        network_focus_.Configure(NetworkCount(), initial_focus);
    }

    const int default_focus = navigation_model_.IndexOfRole(
        page_navigation::NavigationItemRole::kWifiPageNetworkList);
    focus_.Configure(navigation_model_.item_count,
                     std::clamp(focus_.index() >= 0 ? focus_.index() : default_focus,
                                0,
                                navigation_model_.item_count - 1));
}

void WifiPageCoordinator::Show()
{
    ResetTransientState();
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(
                         page_navigation::NavigationItemRole::kWifiPageNetworkList));
}

bool WifiPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    if (network_list_active_) {
        return network_focus_.Move(delta);
    }
    return focus_.Move(delta);
}

bool WifiPageCoordinator::MoveFocusByPage(int delta, int page_size)
{
    if (delta == 0) {
        return false;
    }
    if (!network_list_active_ || page_size <= 0) {
        return MoveFocus(delta);
    }

    const int total_items = NetworkCount();
    if (total_items <= 0) {
        return false;
    }

    const int current_index = network_focus_.index() >= 0 ? network_focus_.index() : 0;
    const int next_index =
        delta > 0 ? NextPageIndex(current_index, total_items, page_size)
                  : PreviousPageIndex(current_index, total_items, page_size);
    return network_focus_.SetIndex(next_index);
}

bool WifiPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool WifiPageCoordinator::EnterNetworkList()
{
    if (network_list_active_ || networks_.empty()) {
        return false;
    }
    network_list_active_ = true;
    const int selected_index =
        SelectedNetworkIndex() != epaper_ui::kNetworkListNoSelection ? SelectedNetworkIndex() : 0;
    network_focus_.Configure(NetworkCount(), selected_index);
    return true;
}

bool WifiPageCoordinator::ExitNetworkList()
{
    if (!network_list_active_) {
        return false;
    }
    network_list_active_ = false;
    return true;
}

bool WifiPageCoordinator::FocusNetworkRow(int index)
{
    if (index < 0 || index >= NetworkCount()) {
        return false;
    }
    const int focus_index = navigation_model_.IndexOfRole(
        page_navigation::NavigationItemRole::kWifiPageNetworkList);
    (void)focus_.SetIndex(focus_index);
    network_list_active_ = false;
    network_focus_.Configure(NetworkCount(), index);
    return true;
}

bool WifiPageCoordinator::SelectNetworkRow(int index)
{
    if (index < 0 || index >= NetworkCount()) {
        return false;
    }
    selected_network_index_ = index;
    network_list_active_ = false;
    network_focus_.Configure(NetworkCount(), index);
    return true;
}

bool WifiPageCoordinator::TogglePasswordVisibility()
{
    password_input_state_.password_visible = !password_input_state_.password_visible;
    return password_input_state_.password_visible;
}

bool WifiPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

bool WifiPageCoordinator::IsPasswordInputFocused() const
{
    return IsRoleFocused(page_navigation::NavigationItemRole::kWifiPagePasswordInput);
}

void WifiPageCoordinator::ResetTransientState()
{
    network_list_active_ = false;
    password_input_state_.focused = false;
    password_input_state_.visibility_button_focused = false;
    password_input_state_.active = false;
}

epaper_ui::WifiPageState WifiPageCoordinator::BuildState() const
{
    epaper_ui::WifiPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.title_text = "WiFi Setup";
    state.network_list = {
        .status = network_status_,
        .focused =
            IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageNetworkList) ||
            network_list_active_,
        .active = network_list_active_,
        .focus_ring_visible = network_list_focus_ring_visible_,
        .focused_network_index = FocusedNetworkIndex(),
        .selected_network_index = SelectedNetworkIndex(),
    };
    state.network_list.networks.reserve(networks_.size());
    for (const NetworkEntry& network : networks_) {
        state.network_list.networks.push_back({
            .ssid_text = network.ssid,
            .current_network = network.current_network,
            .private_network = network.private_network,
            .signal_strength = network.signal_strength,
        });
    }

    state.password_input = password_input_state_;
    state.password_input.focused =
        IsRoleFocused(page_navigation::NavigationItemRole::kWifiPagePasswordInput) ||
        IsRoleFocused(page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton) ||
        password_input_state_.active;
    state.password_input.visibility_button_focused =
        IsRoleFocused(page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton);
    if (!state.password_input.focused) {
        state.password_input.active = false;
    }

    state.scan_button = {
        .label_text = "Scan",
        .selected = IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageScanButton),
    };
    state.connect_button = {
        .label_text = SelectedNetworkIsCurrent() ? "Disconnect" : "Connect",
        .selected = IsRoleFocused(page_navigation::NavigationItemRole::kWifiPageConnectButton),
    };
    return state;
}

epaper_ui::PasswordInputState& WifiPageCoordinator::password_input_state()
{
    return password_input_state_;
}

const epaper_ui::PasswordInputState& WifiPageCoordinator::password_input_state() const
{
    return password_input_state_;
}

bool WifiPageCoordinator::network_list_active() const
{
    return network_list_active_;
}

bool WifiPageCoordinator::HasNetworks() const
{
    return !networks_.empty();
}

int WifiPageCoordinator::FocusedNetworkIndex() const
{
    return network_list_active_ && network_focus_.index() >= 0 &&
                   network_focus_.index() < NetworkCount()
               ? network_focus_.index()
               : epaper_ui::kNetworkListNoSelection;
}

int WifiPageCoordinator::SelectedNetworkIndex() const
{
    return selected_network_index_ >= 0 && selected_network_index_ < NetworkCount()
               ? selected_network_index_
               : epaper_ui::kNetworkListNoSelection;
}

std::string WifiPageCoordinator::SelectedNetworkSsid() const
{
    const int selected_index = SelectedNetworkIndex();
    if (selected_index == epaper_ui::kNetworkListNoSelection) {
        return {};
    }
    return networks_[static_cast<size_t>(selected_index)].ssid;
}

bool WifiPageCoordinator::SelectedNetworkIsCurrent() const
{
    const int selected_index = SelectedNetworkIndex();
    return selected_index != epaper_ui::kNetworkListNoSelection &&
           networks_[static_cast<size_t>(selected_index)].current_network;
}

bool WifiPageCoordinator::CommitFocusedNetworkSelection()
{
    const int focused_index = FocusedNetworkIndex();
    if (focused_index == epaper_ui::kNetworkListNoSelection) {
        return false;
    }
    selected_network_index_ = focused_index;
    return true;
}

void WifiPageCoordinator::InitializePasswordInput()
{
    password_input_state_.label_text = kPasswordLabel;
    password_input_state_.placeholder_text = kPasswordPlaceholder;
    password_input_state_.submit_style = epaper_ui::KeyboardInputSubmitStyle::kJoin;
}

int WifiPageCoordinator::NetworkCount() const
{
    return static_cast<int>(networks_.size());
}
