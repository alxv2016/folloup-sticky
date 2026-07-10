#ifndef ONBOARDING_PAGE_INTERACTIONS_H_
#define ONBOARDING_PAGE_INTERACTIONS_H_

#include <cstdint>
#include <functional>

#include "onboarding_page_coordinator.h"
#include "page_action_result.h"

namespace onboarding_page_interactions {

enum class ActivateIntent : uint8_t {
    kNone = 0,
    kDismiss,  // Close: finish onboarding and go to the dashboard
};

struct ActivateResult {
    ActivateIntent intent = ActivateIntent::kNone;
    bool handled = false;
    bool play_activate_cue = false;
    bool apply_page_state = false;
};

using FocusMoveResult = page_actions::FocusMoveOutcome;

struct ActivateCallbacks {
    std::function<void()> dismiss;
};

// OK on the focused control: Close dismisses; Prev/Next change the active slide in place.
ActivateResult HandlePrimaryActivate(OnboardingPageCoordinator& coordinator);
void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks);
FocusMoveResult HandleMoveFocus(OnboardingPageCoordinator& coordinator, int delta);

}  // namespace onboarding_page_interactions

#endif  // ONBOARDING_PAGE_INTERACTIONS_H_
