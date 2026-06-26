#ifndef DASHBOARD_PAGE_INTERACTIONS_H_
#define DASHBOARD_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "dashboard_page_coordinator.h"
#include "page_action_result.h"

namespace dashboard_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kOpenMenuItem,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    int menu_index = -1;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void(int menu_index)> open_menu_item;
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
};

ActivateResult HandlePrimaryActivate(DashboardPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(DashboardPageCoordinator& coordinator, int delta);

}  // namespace dashboard_page_interactions

#endif  // DASHBOARD_PAGE_INTERACTIONS_H_
