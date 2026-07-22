#ifndef DETAILS_PAGE_INTERACTIONS_H_
#define DETAILS_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "details_page_coordinator.h"
#include "page_action_result.h"

namespace details_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
    kShowPreviousPage,
    kTranscribe,
    kPlayRecording,
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
    std::function<void()> show_previous_page;
    std::function<void()> transcribe;
    std::function<void()> play;
};

ActivateResult HandlePrimaryActivate(DetailsPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(DetailsPageCoordinator& coordinator, int delta);

}  // namespace details_page_interactions

#endif  // DETAILS_PAGE_INTERACTIONS_H_
