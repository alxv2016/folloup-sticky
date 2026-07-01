#ifndef VIBE_CHECK_PAGE_RUNTIME_H_
#define VIBE_CHECK_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "vibe_check_page_interactions.h"

namespace vibe_check_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
vibe_check_page_interactions::ActivateResult ActivateFocusedItem();

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target);
vibe_check_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Reload ideas from the archive (SD I/O; call from a non-UI task). When active, refreshes.
esp_err_t SyncFromService(bool request_refresh_if_active);

// Leave the active card's action row (back to card/footer navigation). Returns true when a
// card was active and got exited; schedules a refresh. Used by the DOWN double-click gesture
// since this device has no dedicated back key.
bool ExitFocusedCard();

// Card action effects, invoked from the page-input activation callbacks. Each performs any
// archive-side work and schedules a page refresh.
void EnterFocusedCard();
void RefreshIdea();
void DeleteCurrentIdea();
void PinCurrentIdea();

}  // namespace vibe_check_page_runtime

#endif  // VIBE_CHECK_PAGE_RUNTIME_H_
