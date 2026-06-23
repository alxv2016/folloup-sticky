#ifndef BUTTON_INPUT_RUNTIME_H_
#define BUTTON_INPUT_RUNTIME_H_

#include <functional>

#include "button_service.h"
#include "esp_err.h"

namespace button_input_runtime {

using DispatchHandler = std::function<void(const button_service::ButtonEventInfo&)>;

esp_err_t Init();
void HandleHardwareEvent(const button_service::ButtonEventInfo& event,
                         DispatchHandler dispatch_handler);

}  // namespace button_input_runtime

#endif  // BUTTON_INPUT_RUNTIME_H_
