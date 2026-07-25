#ifndef WIFI_PAGE_RUNTIME_H_
#define WIFI_PAGE_RUNTIME_H_

#include <cstdint>

#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_action_result.h"
#include "wifi_page_interactions.h"

namespace wifi_page_runtime {

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
esp_err_t ShowPasswordKeyboard();
page_actions::FocusMoveOutcome MoveFocus(int delta, bool page_jump = false);
bool EnterNetworkList();
bool CommitNetworkListSelectionAndExit();
bool ExitNetworkListWithoutSelection();
wifi_page_interactions::ActivateResult ActivateFocusedItem();
wifi_page_interactions::SecondaryActivateResult SecondaryActivateFocusedItem();

footer_runtime::ProjectionState BuildFooterProjectionState();
page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item);
void ResetFocus();
esp_err_t SyncFromService(bool request_refresh_if_active);

}  // namespace wifi_page_runtime

#endif  // WIFI_PAGE_RUNTIME_H_
