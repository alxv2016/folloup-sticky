#ifndef DASHBOARD_PAGE_RUNTIME_H_
#define DASHBOARD_PAGE_RUNTIME_H_

#include "app_interaction_target.h"
#include "dashboard_page_interactions.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"

namespace dashboard_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
page_actions::FocusMoveOutcome MoveFocus(int delta);
dashboard_page_interactions::ActivateResult ActivateFocusedItem();
bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
page_actions::FocusUpdateOutcome FocusTouchTarget(
    const app_interaction::InteractiveTarget& target);
dashboard_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target);
footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();
esp_err_t SyncFromService(bool request_refresh_if_active);
// Repaints the dashboard when the welcome-message rotation interval has rolled over since the
// last sync. Cheap no-op within the same interval; call it from the periodic clock tick.
esp_err_t RefreshWelcomeIfRotated();

// Menu activation. A menu item first offers itself to the registered handler (installed by
// app_shell to route items to their pages); items the handler declines fall back to a
// "coming soon" toast until their destination pages exist.
using MenuItemHandler = bool (*)(int menu_index, void* context);
void SetMenuItemHandler(MenuItemHandler handler, void* context);
void OpenMenuItem(int menu_index);

}  // namespace dashboard_page_runtime

#endif  // DASHBOARD_PAGE_RUNTIME_H_
