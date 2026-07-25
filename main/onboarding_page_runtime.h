#ifndef ONBOARDING_PAGE_RUNTIME_H_
#define ONBOARDING_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "onboarding_page_interactions.h"
#include "page_action_result.h"

namespace onboarding_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);

page_actions::FocusMoveOutcome MoveFocus(int delta);
onboarding_page_interactions::ActivateResult ActivateFocusedItem();

// Reset to the first slide with the Close button focused (call on page entry).
void ResetFocus();

// Close is deferred so the screen change happens after input dispatch returns. Activation calls
// RequestDismiss(); app_shell polls ConsumePendingDismiss() to finish onboarding + show the dashboard.
void RequestDismiss();
bool ConsumePendingDismiss();

// Manual (re)launch from Settings, deferred the same way. Unlike first-run onboarding this does NOT
// touch the "onboarded" NVS flag; app_shell polls ConsumePendingManualLaunch() to show the carousel
// and returns to Settings when it is dismissed.
void RequestManualLaunch();
bool ConsumePendingManualLaunch();

}  // namespace onboarding_page_runtime

#endif  // ONBOARDING_PAGE_RUNTIME_H_
