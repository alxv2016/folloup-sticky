#ifndef TIME_PAGE_RUNTIME_H_
#define TIME_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "page_navigation/navigation_model.h"
#include "time_page_interactions.h"

namespace time_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
page_actions::FocusMoveOutcome MoveFocus(int delta);
time_page_interactions::ActivateResult ActivateFocusedItem();
bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(
    const app_interaction::InteractiveTarget& target);
time_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);
footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();
esp_err_t SyncFromService(bool request_refresh_if_active);

// Field activations (invoked by the page-input activate callbacks).
void ToggleMeridiem();
esp_err_t Save();
esp_err_t ShowTimezoneModal();
esp_err_t ShowFieldKeyboard(page_navigation::NavigationItemRole field);

// Routes a select-modal submission to the timezone field when the timezone modal is the
// one currently open. Returns true if it consumed the submission. Cleared by
// ClearPendingSelectModal when another select modal takes over.
bool HandleSelectModalSubmit(int selected_index);
void ClearPendingSelectModal();

}  // namespace time_page_runtime

#endif  // TIME_PAGE_RUNTIME_H_
