#ifndef WIFI_PAGE_RUNTIME_H_
#define WIFI_PAGE_RUNTIME_H_

#include <cstdint>

#include "app_interaction_result.h"
#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_interaction_runtime.h"

namespace wifi_page_runtime {

using RefreshHandler = esp_err_t (*)(display_service::RefreshMode refresh_mode, void* context);

enum class FocusItem : uint8_t {
    kNone = 0,
    kNetworkList,
    kPasswordInput,
    kPasswordVisibility,
    kScanButton,
    kConnectButton,
    kFooterWifi,
    kFooterSettings,
    kFooterHome,
};

struct ActivationResult {
    bool handled = false;
    bool play_feedback = false;
    app_interaction::FeedbackCue feedback_cue = app_interaction::FeedbackCue::kNone;
    footer_runtime::FooterFocusItem footer_item = footer_runtime::FooterFocusItem::kNone;
};

esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
bool MoveFocus(int delta);
bool EnterNetworkList();
bool CommitNetworkListSelectionAndExit();
bool ExitNetworkListWithoutSelection();
ActivationResult ActivateFocusedItem();
bool SecondaryActivateFocusedItem();
footer_runtime::ProjectionState BuildFooterProjectionState();
bool SyncFocusFromFooterProjection();
void ResetFocus();
void SetRefreshHandler(RefreshHandler handler, void* context);
esp_err_t SyncFromService(bool request_refresh_if_active);
page_interaction_runtime::TouchProvider BuildTouchProvider();

}  // namespace wifi_page_runtime

#endif  // WIFI_PAGE_RUNTIME_H_
