#ifndef WIFI_PAGE_COORDINATOR_H_
#define WIFI_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/password_input.h"
#include "epaper_ui/wifi_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "wifi_service.h"

class WifiPageCoordinator {
public:
    WifiPageCoordinator();

    void RefreshFromService(const wifi_service::UiState& ui_state,
                            const wifi_service::ScanSnapshot& scan_snapshot);
    void Show();
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool EnterNetworkList();
    bool ExitNetworkList();
    bool FocusNetworkRow(int index);
    bool SelectNetworkRow(int index);
    bool TogglePasswordVisibility();
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    bool IsPasswordInputFocused() const;
    void ResetTransientState();

    epaper_ui::WifiPageState BuildState() const;

    epaper_ui::PasswordInputState& password_input_state();
    const epaper_ui::PasswordInputState& password_input_state() const;
    bool network_list_active() const;
    bool HasNetworks() const;
    int FocusedNetworkIndex() const;
    int SelectedNetworkIndex() const;
    std::string SelectedNetworkSsid() const;
    bool SelectedNetworkIsCurrent() const;
    bool CommitFocusedNetworkSelection();
    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    struct NetworkEntry {
        std::string ssid = {};
        bool current_network = false;
        bool private_network = false;
        epaper_ui::NetworkSignalStrength signal_strength =
            epaper_ui::NetworkSignalStrength::kStrong;
    };

    void InitializePasswordInput();
    int NetworkCount() const;

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildWifiPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus network_focus_{0, 0};
    epaper_ui::NetworkListStatus network_status_ = epaper_ui::NetworkListStatus::kIdle;
    std::vector<NetworkEntry> networks_ = {};
    bool network_list_active_ = false;
    bool network_list_focus_ring_visible_ = true;
    int selected_network_index_ = epaper_ui::kNetworkListNoSelection;
    epaper_ui::PasswordInputState password_input_state_ = {};
};

#endif  // WIFI_PAGE_COORDINATOR_H_
