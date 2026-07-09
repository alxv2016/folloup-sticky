#ifndef TODOS_PAGE_INTERACTIONS_H_
#define TODOS_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "todos_page_coordinator.h"

namespace todos_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
    kOpenItemActions,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
    std::function<void()> open_item_actions;
};

// Primary (OK / tap-release) on a focused date chip enters its item list; on a focused todo it
// asks to open the item-actions modal; on the footer it routes to a page.
ActivateResult HandlePrimaryActivate(TodosPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(TodosPageCoordinator& coordinator, int delta);

}  // namespace todos_page_interactions

#endif  // TODOS_PAGE_INTERACTIONS_H_
