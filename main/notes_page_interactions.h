#ifndef NOTES_PAGE_INTERACTIONS_H_
#define NOTES_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "notes_page_coordinator.h"
#include "page_action_result.h"

namespace notes_page_interactions {

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

// Primary (OK / tap-release) on a focused date chip enters its item list; on a focused item it
// asks to open the item-actions modal; on the footer it routes to a page.
ActivateResult HandlePrimaryActivate(NotesPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(NotesPageCoordinator& coordinator, int delta);

}  // namespace notes_page_interactions

#endif  // NOTES_PAGE_INTERACTIONS_H_
