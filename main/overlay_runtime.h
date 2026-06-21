#ifndef OVERLAY_RUNTIME_H_
#define OVERLAY_RUNTIME_H_

#include "button_service.h"
#include "epaper_ui/select_modal.h"
#include "epaper_ui/shutdown_modal.h"
#include "epaper_ui/toast.h"
#include "esp_err.h"
#include "touch_service.h"

namespace overlay_runtime {

struct InputResult {
    bool consumed = false;
    bool request_shutdown = false;
    bool select_modal_submitted = false;
    int select_modal_selected_index = -1;
};

esp_err_t Init();
bool IsShutdownModalVisible();
bool IsSelectModalVisible();
bool IsShutdownPending();
bool IsInputCaptured();

esp_err_t ShowShutdownModal();
esp_err_t DismissShutdownModal();
esp_err_t ShowSelectModal(const epaper_ui::SelectModalState& state);
esp_err_t DismissSelectModal();
bool MoveFocus(int delta);
void SetShutdownRequestInProgress(bool in_progress);

InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event);
InputResult HandleTouchEvent(const touch_service::TouchEventInfo& event);

esp_err_t ShowToast(const epaper_ui::ToastState& state);
esp_err_t ShowToastForDuration(const epaper_ui::ToastState& state, uint32_t duration_ms);
esp_err_t ClearToast();

}  // namespace overlay_runtime

#endif  // OVERLAY_RUNTIME_H_
