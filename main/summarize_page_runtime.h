#ifndef SUMMARIZE_PAGE_RUNTIME_H_
#define SUMMARIZE_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "summarize_page_interactions.h"
#include "summary_service.h"

namespace summarize_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
summarize_page_interactions::ActivateResult ActivateFocusedItem();

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target);
summarize_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Load the current summary snapshot from the service and refresh (call on page entry).
esp_err_t SyncFromService(bool request_refresh_if_active);
// Adopt a summary snapshot delivered by an event (avoids re-entering the service's lock) and
// refresh if the page is active. app_shell calls this from the summary event handler.
esp_err_t OnSummarySnapshot(const summary_service::Snapshot& snapshot, bool request_refresh);

// Activation effects invoked from the page-input callbacks.
void ToggleSegment();
void EnterScroll();
void RequestNotesSummary();
void RequestTodosSummary();
// Leave an entered segment/scroll control (DOWN double-click). Returns true if one was active.
bool ExitActiveControl();

}  // namespace summarize_page_runtime

#endif  // SUMMARIZE_PAGE_RUNTIME_H_
