#ifndef INPUT_RUNTIME_SETUP_H_
#define INPUT_RUNTIME_SETUP_H_

#include "button_service.h"
#include "esp_err.h"
#include "touch_service.h"

namespace input_runtime_setup {

using InputsEnabledProvider = bool (*)(void* context);

struct Dependencies {
    InputsEnabledProvider inputs_enabled = nullptr;
    void* inputs_enabled_context = nullptr;
    button_service::EventHandler button_handler = nullptr;
    void* button_handler_context = nullptr;
    touch_service::EventHandler touch_handler = nullptr;
    void* touch_handler_context = nullptr;
};

void Configure(const Dependencies& dependencies);
esp_err_t InitButtonInput();
esp_err_t InitTouchInput();

}  // namespace input_runtime_setup

#endif  // INPUT_RUNTIME_SETUP_H_
