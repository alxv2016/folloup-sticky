#ifndef SETTINGS_PAGE_INTERACTIONS_H_
#define SETTINGS_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "settings_page_coordinator.h"

namespace settings_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowWifi,
    kShowTime,
    kForceRefresh,
    kToggleWifi,
    kToggleAccessPoint,
    kShowFormatSdModal,
    kShowOnboarding,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
    std::function<void()> force_refresh;
    std::function<void()> toggle_wifi;
    std::function<void()> toggle_access_point;
    std::function<void()> show_format_sd_modal;
    std::function<void()> show_onboarding;
};

ActivateResult HandlePrimaryActivate(const SettingsPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result,
                               const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(SettingsPageCoordinator& coordinator, int delta);

}  // namespace settings_page_interactions

#endif  // SETTINGS_PAGE_INTERACTIONS_H_
