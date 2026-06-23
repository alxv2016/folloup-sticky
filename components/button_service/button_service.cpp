#include "button_service.h"

#include <cstdint>

#include "button_gpio.h"
#include "esp_log.h"
#include "iot_button.h"
#include "sticky_board_config.h"

namespace button_service {
namespace {

constexpr const char* kTag = "ButtonService";
constexpr uint16_t kShortPressMs = 180;
constexpr uint16_t kLongPressMs = 2000;

struct ButtonContext {
    const char* label = nullptr;
    ButtonId id = ButtonId::kPowerOk;
    gpio_num_t gpio = GPIO_NUM_NC;
    button_handle_t handle = nullptr;
};

ButtonContext s_buttons[] = {
    {"POWER_OK", ButtonId::kPowerOk, STICKY_POWER_BUTTON_PIN, nullptr},
    {"UP", ButtonId::kUp, STICKY_BUTTON_UP_PIN, nullptr},
    {"DOWN", ButtonId::kDown, STICKY_BUTTON_DOWN_PIN, nullptr},
};

bool s_initialized = false;
EventHandler s_event_handler = nullptr;
void* s_event_handler_context = nullptr;

const char* EventName(button_event_t event)
{
    switch (event) {
        case BUTTON_PRESS_DOWN:
            return "PRESS_DOWN";
        case BUTTON_PRESS_UP:
            return "PRESS_UP";
        case BUTTON_PRESS_REPEAT:
            return "PRESS_REPEAT";
        case BUTTON_SINGLE_CLICK:
            return "SINGLE_CLICK";
        case BUTTON_DOUBLE_CLICK:
            return "DOUBLE_CLICK";
        case BUTTON_LONG_PRESS_START:
            return "LONG_PRESS_START";
        case BUTTON_LONG_PRESS_UP:
            return "LONG_PRESS_UP";
        default:
            return iot_button_get_event_str(event);
    }
}

bool ToButtonEvent(button_event_t event, ButtonEvent* out_event)
{
    if (out_event == nullptr) {
        return false;
    }

    switch (event) {
        case BUTTON_PRESS_DOWN:
            *out_event = ButtonEvent::kPressDown;
            return true;
        case BUTTON_PRESS_UP:
            *out_event = ButtonEvent::kPressUp;
            return true;
        case BUTTON_PRESS_REPEAT:
            *out_event = ButtonEvent::kPressRepeat;
            return true;
        case BUTTON_SINGLE_CLICK:
            *out_event = ButtonEvent::kSingleClick;
            return true;
        case BUTTON_DOUBLE_CLICK:
            *out_event = ButtonEvent::kDoubleClick;
            return true;
        case BUTTON_LONG_PRESS_START:
            *out_event = ButtonEvent::kLongPressStart;
            return true;
        case BUTTON_LONG_PRESS_UP:
            *out_event = ButtonEvent::kLongPressUp;
            return true;
        default:
            return false;
    }
}

void ButtonEventCallback(void* button_handle, void* user_data)
{
    const auto* context = static_cast<const ButtonContext*>(user_data);
    const button_event_t event =
        iot_button_get_event(static_cast<button_handle_t>(button_handle));
    const uint32_t pressed_ms =
        iot_button_get_pressed_time(static_cast<button_handle_t>(button_handle));

    ESP_LOGI(kTag, "%s event=%s gpio=%d pressed_ms=%lu",
             context != nullptr ? context->label : "UNKNOWN",
             EventName(event),
             context != nullptr ? context->gpio : GPIO_NUM_NC,
             static_cast<unsigned long>(pressed_ms));

    ButtonEvent app_event = ButtonEvent::kPressDown;
    if (context != nullptr && s_event_handler != nullptr &&
        ToButtonEvent(event, &app_event)) {
        ButtonEventInfo event_info = {};
        event_info.button = context->id;
        event_info.event = app_event;
        event_info.pressed_ms = pressed_ms;
        s_event_handler(event_info, s_event_handler_context);
    }
}

esp_err_t RegisterEvent(ButtonContext* context, button_event_t event)
{
    if (context == nullptr || context->handle == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = iot_button_register_cb(context->handle, event, nullptr,
                                           ButtonEventCallback, context);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Register %s callback for %s failed: %s",
                 EventName(event), context->label, esp_err_to_name(err));
    }
    return err;
}

esp_err_t CreateButton(ButtonContext* context)
{
    if (context == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    button_config_t button_config = {};
    button_config.long_press_time = kLongPressMs;
    button_config.short_press_time = kShortPressMs;

    button_gpio_config_t gpio_config = {};
    gpio_config.gpio_num = context->gpio;
    gpio_config.active_level = 0;
    gpio_config.enable_power_save = false;
    gpio_config.disable_pull = false;

    esp_err_t err = iot_button_new_gpio_device(&button_config, &gpio_config,
                                               &context->handle);
    if (err != ESP_OK || context->handle == nullptr) {
        ESP_LOGE(kTag, "Create %s button on GPIO%d failed: %s",
                 context->label, context->gpio, esp_err_to_name(err));
        context->handle = nullptr;
        return err == ESP_OK ? ESP_FAIL : err;
    }

    ESP_LOGI(kTag, "Created %s button on GPIO%d", context->label, context->gpio);

    esp_err_t first_error = ESP_OK;
    const button_event_t events[] = {
        BUTTON_PRESS_DOWN,
        BUTTON_PRESS_UP,
        BUTTON_PRESS_REPEAT,
        BUTTON_SINGLE_CLICK,
        BUTTON_DOUBLE_CLICK,
        BUTTON_LONG_PRESS_START,
        BUTTON_LONG_PRESS_UP,
    };
    for (button_event_t event : events) {
        err = RegisterEvent(context, event);
        if (first_error == ESP_OK && err != ESP_OK) {
            first_error = err;
        }
    }

    return first_error;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t first_error = ESP_OK;
    for (ButtonContext& button : s_buttons) {
        esp_err_t err = CreateButton(&button);
        if (first_error == ESP_OK && err != ESP_OK) {
            first_error = err;
        }
    }

    if (first_error != ESP_OK) {
        return first_error;
    }

    s_initialized = true;
    ESP_LOGI(kTag, "Button service initialized");
    return ESP_OK;
}

void SetEventHandler(EventHandler handler, void* context)
{
    s_event_handler = handler;
    s_event_handler_context = context;
}

}  // namespace button_service
