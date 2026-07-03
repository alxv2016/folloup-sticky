#ifndef DETAILS_PAGE_RUNTIME_H_
#define DETAILS_PAGE_RUNTIME_H_

#include <string>

#include "app_interaction_target.h"
#include "details_page_coordinator.h"
#include "details_page_interactions.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"

namespace details_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);

page_actions::FocusMoveOutcome MoveFocus(int delta);
details_page_interactions::ActivateResult ActivateFocusedItem();

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target);
details_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();

// Stash the recording to show, then read the archive (SD) and (re)build the page.
void QueueShow(const std::string& recording_id, DetailsPageSource source_page);
esp_err_t SyncFromArchive(bool request_refresh_if_active);

// The page that opened Details, so the Back button can return to it.
DetailsPageSource SourcePage();

// Back navigation is deferred so it happens after input dispatch returns (avoids changing screens
// mid-dispatch). The Back activation calls RequestBack(); app_shell polls ConsumePendingBack().
void RequestBack();
bool ConsumePendingBack();

// Leave the entered scroll container (DOWN double-click). Returns true if it was active.
bool ExitActiveControl();

}  // namespace details_page_runtime

#endif  // DETAILS_PAGE_RUNTIME_H_
