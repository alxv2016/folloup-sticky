#ifndef SETTINGS_PAGE_RUNTIME_H_
#define SETTINGS_PAGE_RUNTIME_H_

#include <cstdint>

#include "app_interaction_result.h"
#include "app_interaction_target.h"
#include "display_service.h"
#include "esp_err.h"
#include "footer_runtime.h"
#include "page_interaction_runtime.h"

namespace settings_page_runtime {

enum class FocusItem : uint8_t {
    kNone = 0,
    kWifiToggle,
    kAccessPointToggle,
    kFormatSdButton,
    kFooterSettings,
    kFooterHome,
};

enum class ActionRequest : uint8_t {
    kNone = 0,
    kShowFormatSdModal,
};

using ActionHandler = void (*)(ActionRequest request, void* context);

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
ActivationResult ActivateFocusedItem();
footer_runtime::ProjectionState BuildFooterProjectionState();
bool SyncFocusFromFooterProjection();
void ResetFocus();
void SetActionHandler(ActionHandler handler, void* context);
page_interaction_runtime::TouchProvider BuildTouchProvider();

}  // namespace settings_page_runtime

#endif  // SETTINGS_PAGE_RUNTIME_H_
