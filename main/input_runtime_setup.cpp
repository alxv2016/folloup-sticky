#include "input_runtime_setup.h"

#include <mutex>

#include "button_input_runtime.h"

namespace input_runtime_setup {
namespace {

std::mutex s_mutex;
Dependencies s_dependencies = {};

bool InputsEnabled()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_dependencies.inputs_enabled == nullptr ||
           s_dependencies.inputs_enabled(s_dependencies.inputs_enabled_context);
}

void HandleButtonEvent(const button_service::ButtonEventInfo& event, void*)
{
    button_service::EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_dependencies.button_handler;
        context = s_dependencies.button_handler_context;
    }

    if (handler == nullptr || !InputsEnabled()) {
        return;
    }

    handler(event, context);
}

}  // namespace

void Configure(const Dependencies& dependencies)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_dependencies = dependencies;
    }

    button_service::SetEventHandler(&HandleButtonEvent, nullptr);
}

esp_err_t InitButtonInput()
{
    const esp_err_t runtime_err = button_input_runtime::Init();
    if (runtime_err != ESP_OK) {
        return runtime_err;
    }

    return button_service::Init();
}

}  // namespace input_runtime_setup
