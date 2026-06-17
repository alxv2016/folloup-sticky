#include "app_shell.h"

#include "button_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_service.h"

namespace app_shell {
namespace {

constexpr const char* kTag = "AppShell";
constexpr bool kEnablePowerButtonShutdown = true;
constexpr uint32_t kShutdownTaskStackWords = 3072;
constexpr UBaseType_t kShutdownTaskPriority = 5;
constexpr TickType_t kPowerButtonReleaseSettleDelay = pdMS_TO_TICKS(500);

TaskHandle_t s_shutdown_task = nullptr;
bool s_power_button_shutdown_pending = false;

void ConfirmPendingOtaImage()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;

    if (running != nullptr &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}

const char* ButtonIdName(button_service::ButtonId button)
{
    switch (button) {
        case button_service::ButtonId::kPowerOk:
            return "POWER_OK";
        case button_service::ButtonId::kUp:
            return "UP";
        case button_service::ButtonId::kDown:
            return "DOWN";
        default:
            return "UNKNOWN";
    }
}

const char* ButtonEventName(button_service::ButtonEvent event)
{
    switch (event) {
        case button_service::ButtonEvent::kPressDown:
            return "PRESS_DOWN";
        case button_service::ButtonEvent::kPressUp:
            return "PRESS_UP";
        case button_service::ButtonEvent::kSingleClick:
            return "SINGLE_CLICK";
        case button_service::ButtonEvent::kDoubleClick:
            return "DOUBLE_CLICK";
        case button_service::ButtonEvent::kLongPressStart:
            return "LONG_PRESS_START";
        case button_service::ButtonEvent::kLongPressUp:
            return "LONG_PRESS_UP";
        default:
            return "UNKNOWN";
    }
}

void HandleButtonEvent(const button_service::ButtonEventInfo& event, void*)
{
    ESP_LOGI(kTag, "Button intent: button=%s event=%s pressed_ms=%lu",
             ButtonIdName(event.button), ButtonEventName(event.event),
             static_cast<unsigned long>(event.pressed_ms));

    if (event.button != button_service::ButtonId::kPowerOk) {
        return;
    }

    if (event.event == button_service::ButtonEvent::kLongPressStart) {
        s_power_button_shutdown_pending = true;
        ESP_LOGW(kTag, "Power button long press detected; release to shutdown");
        return;
    }

    if (event.event != button_service::ButtonEvent::kLongPressUp ||
        !s_power_button_shutdown_pending) {
        return;
    }

    s_power_button_shutdown_pending = false;

    if constexpr (!kEnablePowerButtonShutdown) {
        ESP_LOGW(kTag, "Power button released after long press; shutdown is disabled");
        return;
    }

    if (s_shutdown_task == nullptr) {
        ESP_LOGW(kTag, "Power button released after long press; shutdown task unavailable");
        return;
    }

    ESP_LOGW(kTag, "Power button released after long press; requesting shutdown");
    xTaskNotifyGive(s_shutdown_task);
}

void ShutdownTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(kTag, "Shutdown request accepted; waiting for power button release settle");
        vTaskDelay(kPowerButtonReleaseSettleDelay);
        ESP_LOGW(kTag, "Power button release settled; releasing power hold");
        const esp_err_t err = power_service::RequestShutdown();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Shutdown request failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGW(kTag, "Shutdown request returned; board may still be powered");
        }
    }
}

void StartShutdownTask()
{
    if (s_shutdown_task != nullptr) {
        return;
    }

    const BaseType_t created = xTaskCreate(ShutdownTask, "app_shutdown",
                                          kShutdownTaskStackWords, nullptr,
                                          kShutdownTaskPriority,
                                          &s_shutdown_task);
    if (created != pdPASS) {
        s_shutdown_task = nullptr;
        ESP_LOGW(kTag, "Failed to create shutdown task");
    }
}

void InitButtonService()
{
    button_service::SetEventHandler(HandleButtonEvent, nullptr);
    const esp_err_t err = button_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Button service init failed: %s", esp_err_to_name(err));
    }
}

}  // namespace

void Run()
{
    ESP_ERROR_CHECK(power_service::EnablePowerHold());
    ConfirmPendingOtaImage();
    ESP_ERROR_CHECK(power_service::Init());
    power_service::LogDebugStatus();
    StartShutdownTask();
    InitButtonService();
}

}  // namespace app_shell
