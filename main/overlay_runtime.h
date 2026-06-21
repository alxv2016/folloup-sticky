#ifndef OVERLAY_RUNTIME_H_
#define OVERLAY_RUNTIME_H_

#include "button_service.h"
#include "epaper_ui/shutdown_modal.h"
#include "epaper_ui/toast.h"
#include "esp_err.h"
#include "touch_service.h"

namespace overlay_runtime {

struct InputResult {
    bool consumed = false;
    bool request_shutdown = false;
};

esp_err_t Init();
bool IsShutdownModalVisible();
bool IsShutdownPending();
bool IsInputCaptured();

esp_err_t ShowShutdownModal();
esp_err_t DismissShutdownModal();
void SetShutdownRequestInProgress(bool in_progress);

InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event);
InputResult HandleTouchEvent(const touch_service::TouchEventInfo& event);

esp_err_t ShowToast(const epaper_ui::ToastState& state);
esp_err_t ClearToast();

}  // namespace overlay_runtime

#endif  // OVERLAY_RUNTIME_H_
