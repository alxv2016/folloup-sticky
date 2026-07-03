#ifndef NOTES_PAGE_RUNTIME_H_
#define NOTES_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "notes_page_interactions.h"
#include "page_action_result.h"

namespace notes_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
notes_page_interactions::ActivateResult ActivateFocusedItem();

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target);
notes_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Load the archive (SD read) and (re)build the timeline. Call on page entry / after mutations.
esp_err_t SyncFromArchive(bool request_refresh_if_active);

// Leave an entered item list (DOWN double-click). Returns true if a list was active.
bool ExitActiveControl();

// A value snapshot of the currently selected item, for the item-actions modal (safe to use
// outside the runtime lock).
struct SelectedEntrySnapshot {
    bool valid = false;
    std::string recording_id = {};
    std::string recording_path = {};
    bool follow_up = false;
    bool follow_up_completed = false;
};
SelectedEntrySnapshot GetSelectedEntrySnapshot();

// Optimistically reflect a follow-up change from the modal and repaint.
void SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed);

}  // namespace notes_page_runtime

#endif  // NOTES_PAGE_RUNTIME_H_
