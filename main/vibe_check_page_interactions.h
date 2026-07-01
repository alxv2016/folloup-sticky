#ifndef VIBE_CHECK_PAGE_INTERACTIONS_H_
#define VIBE_CHECK_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "page_action_result.h"
#include "vibe_check_page_coordinator.h"

namespace vibe_check_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kShowHome,
    kShowSettings,
    kShowWifi,
    kShowTime,
    kEnterCard,
    kRefreshIdea,
    kDeleteIdea,
    kPinIdea,
    kTranscribeIdea,
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> show_home;
    std::function<void()> show_settings;
    std::function<void()> show_wifi;
    std::function<void()> show_time;
    std::function<void()> enter_card;
    std::function<void()> refresh_idea;
    std::function<void()> delete_idea;
    std::function<void()> pin_idea;
    std::function<void()> transcribe_idea;
};

ActivateResult HandlePrimaryActivate(VibeCheckPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(VibeCheckPageCoordinator& coordinator, int delta);

}  // namespace vibe_check_page_interactions

#endif  // VIBE_CHECK_PAGE_INTERACTIONS_H_
