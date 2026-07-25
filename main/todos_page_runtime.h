#ifndef TODOS_PAGE_RUNTIME_H_
#define TODOS_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "todos_page_interactions.h"

namespace todos_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
todos_page_interactions::ActivateResult ActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Load the archive (SD read) and (re)build the timeline. Call on page entry / after mutations.
esp_err_t SyncFromArchive(bool request_refresh_if_active);

// Leave an entered item list (DOWN double-click). Returns true if a list was active.
bool ExitActiveControl();

// Open the item-actions modal for the currently selected todo (View details / Follow up /
// Complete / Delete / Close). Returns true if it was shown.
bool ShowItemActionsModal();
// Dispatch the item-actions modal selection. Returns true if an item-actions modal was pending
// (so the shared submit chain in app_shell stops here).
bool HandleItemActionSelection(int selected_index);
// The recording id whose details were requested via the modal (deferred so app_shell opens the
// Details screen after input dispatch returns). Empty when none is pending.
std::string ConsumePendingViewDetails();

// Optimistically reflect a follow-up / completion change from the modal and repaint.
void SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed);
void SetEntryChecked(const std::string& recording_id, bool checked);

}  // namespace todos_page_runtime

#endif  // TODOS_PAGE_RUNTIME_H_
