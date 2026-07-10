#include "onboarding_page_interactions.h"

#include "epaper_ui/onboarding_page.h"

namespace onboarding_page_interactions {

ActivateResult HandlePrimaryActivate(OnboardingPageCoordinator& coordinator)
{
    ActivateResult result = {};
    switch (coordinator.FocusedControl()) {
        case epaper_ui::OnboardingControl::kClose:
            result.intent = ActivateIntent::kDismiss;
            result.handled = true;
            result.play_activate_cue = true;
            break;
        case epaper_ui::OnboardingControl::kPrev:
            result.handled = true;
            if (coordinator.PrevSlide()) {
                result.play_activate_cue = true;
                result.apply_page_state = true;
            }
            break;
        case epaper_ui::OnboardingControl::kNext:
            result.handled = true;
            if (coordinator.NextSlide()) {
                result.play_activate_cue = true;
                result.apply_page_state = true;
            }
            break;
        case epaper_ui::OnboardingControl::kNone:
        default:
            break;
    }
    return result;
}

void ApplyPrimaryActivateResult(const ActivateResult& result, const ActivateCallbacks& callbacks)
{
    if (result.intent == ActivateIntent::kDismiss && callbacks.dismiss) {
        callbacks.dismiss();
    }
}

FocusMoveResult HandleMoveFocus(OnboardingPageCoordinator& coordinator, int delta)
{
    FocusMoveResult result = {};
    if (!coordinator.MoveFocus(delta)) {
        return result;
    }
    result.handled = true;
    result.play_navigation_cue = true;
    result.apply_page_state = true;
    return result;
}

}  // namespace onboarding_page_interactions
