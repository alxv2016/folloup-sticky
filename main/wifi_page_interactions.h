#ifndef WIFI_PAGE_INTERACTIONS_H_
#define WIFI_PAGE_INTERACTIONS_H_

#include <string>

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "wifi_page_coordinator.h"

namespace wifi_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowTime,
    kForceRefresh,
    kOpenPasswordKeyboard,
    kStartNetworkScan,
    kToggleSelectedNetworkConnection,
};

enum class SecondaryActivateIntent : uint8_t {
    kNone = 0,
    kExitNetworkList,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
    bool disconnect_current_network = false;
    std::string connect_ssid = {};
    std::string connect_password = {};
};

struct SecondaryActivateResult {
    SecondaryActivateIntent intent = SecondaryActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_time;
    std::function<void()> force_refresh;
    std::function<void()> open_password_keyboard;
    std::function<void()> start_network_scan;
    std::function<void(bool disconnect_current_network,
                       const std::string& ssid,
                       const std::string& password)> toggle_selected_network_connection;
};

struct SecondaryActivateCallbacks {
    std::function<void()> play_activate_cue;
    std::function<void()> apply_page_state;
};

ActivateResult HandlePrimaryActivate(WifiPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result,
                                const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(WifiPageCoordinator& coordinator,
                                int delta,
                                bool page_jump,
                                int page_size);
ActivateResult HandleNetworkRowActivate(WifiPageCoordinator& coordinator, int row_index);
SecondaryActivateResult HandleSecondaryActivate(WifiPageCoordinator& coordinator);
void ApplySecondaryActivateResult(const SecondaryActivateResult& result,
                                  const SecondaryActivateCallbacks& callbacks);

}  // namespace wifi_page_interactions

#endif  // WIFI_PAGE_INTERACTIONS_H_
