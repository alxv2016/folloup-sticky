#ifndef TIME_PAGE_INTERACTIONS_H_
#define TIME_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "time_page_coordinator.h"

namespace time_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowTimezoneModal,
    kEditHour,
    kEditMinute,
    kToggleMeridiem,
    kEditMonth,
    kEditDay,
    kEditYear,
    kSave,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_timezone_modal;
    std::function<void()> edit_hour;
    std::function<void()> edit_minute;
    std::function<void()> toggle_meridiem;
    std::function<void()> edit_month;
    std::function<void()> edit_day;
    std::function<void()> edit_year;
    std::function<void()> save;
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
};

ActivateResult HandlePrimaryActivate(TimePageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(TimePageCoordinator& coordinator, int delta);

}  // namespace time_page_interactions

#endif  // TIME_PAGE_INTERACTIONS_H_
