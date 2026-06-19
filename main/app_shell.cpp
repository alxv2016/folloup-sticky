#include "app_shell.h"

#include "buzzer_service.h"
#include "button_service.h"
#include "display_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_service.h"
#include "storage_service.h"
#include "touch_service.h"

namespace app_shell {
namespace {

constexpr const char* kTag = "AppShell";
constexpr bool kEnablePowerButtonShutdown = true;
constexpr uint32_t kShutdownTaskStackWords = 3072;
constexpr UBaseType_t kShutdownTaskPriority = 5;
constexpr TickType_t kPowerButtonReleaseSettleDelay = pdMS_TO_TICKS(500);
constexpr TickType_t kTouchContactGap = pdMS_TO_TICKS(300);

TaskHandle_t s_shutdown_task = nullptr;
bool s_power_button_shutdown_pending = false;
display_service::DemoSelection s_demo_selection = display_service::DemoSelection::kTop;
bool s_touch_contact_active = false;
TickType_t s_last_touch_event_tick = 0;

void PlayBuzzerPattern(buzzer_service::Pattern pattern, const char* name)
{
    const esp_err_t err = buzzer_service::PlayPattern(pattern);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Buzzer %s pattern failed: %s", name, esp_err_to_name(err));
    }
}

void RequestDemoSelection(display_service::DemoSelection selection, const char* source)
{
    const esp_err_t err = display_service::SelectDemoSelection(selection);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Display demo selection from %s failed: %s",
                 source, esp_err_to_name(err));
        return;
    }

    if (err == ESP_OK) {
        s_demo_selection = selection;
    }
}

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

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            PlayBuzzerPattern(buzzer_service::Pattern::kClick, "click");
            if (event.button == button_service::ButtonId::kUp ||
                event.button == button_service::ButtonId::kDown) {
                const display_service::DemoSelection selection =
                    event.button == button_service::ButtonId::kUp
                        ? display_service::DemoSelection::kTop
                        : display_service::DemoSelection::kBottom;
                RequestDemoSelection(selection, "button");
            }
            break;
        case button_service::ButtonEvent::kDoubleClick:
            PlayBuzzerPattern(buzzer_service::Pattern::kDoubleClick, "double-click");
            break;
        case button_service::ButtonEvent::kLongPressStart:
            PlayBuzzerPattern(buzzer_service::Pattern::kLongClick, "long-click");
            break;
        default:
            break;
    }

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

void HandleTouchEvent(const touch_service::TouchEventInfo& event, void*)
{
    ESP_LOGI(kTag, "Touch intent: count=%u", static_cast<unsigned>(event.count));
    if (event.count > 0) {
        const TickType_t now = xTaskGetTickCount();
        const bool new_contact =
            !s_touch_contact_active || now - s_last_touch_event_tick >= kTouchContactGap;
        s_touch_contact_active = true;
        s_last_touch_event_tick = now;

        if (new_contact) {
            const display_service::DemoSelection next_selection =
                s_demo_selection == display_service::DemoSelection::kTop
                    ? display_service::DemoSelection::kBottom
                    : display_service::DemoSelection::kTop;
            ESP_LOGI(kTag, "Touch toggles display demo selection to %s",
                     next_selection == display_service::DemoSelection::kTop
                         ? "top"
                         : "bottom");
            RequestDemoSelection(next_selection, "touch");
            PlayBuzzerPattern(buzzer_service::Pattern::kClick, "touch");
        }
    }

    for (uint8_t i = 0; i < event.count; ++i) {
        ESP_LOGI(kTag, "Touch intent point[%u]: x=%u y=%u size=%u id=%u",
                 static_cast<unsigned>(i), static_cast<unsigned>(event.points[i].x),
                 static_cast<unsigned>(event.points[i].y),
                 static_cast<unsigned>(event.points[i].size),
                 static_cast<unsigned>(event.points[i].id));
    }
}

void ShutdownTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(kTag, "Shutdown request accepted; waiting for power button release settle");
        PlayBuzzerPattern(buzzer_service::Pattern::kShutdown, "shutdown");
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

void InitBuzzerService()
{
    const esp_err_t err = buzzer_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Buzzer service init failed: %s", esp_err_to_name(err));
        return;
    }

    const esp_err_t play_err =
        buzzer_service::PlayPattern(buzzer_service::Pattern::kStartup);
    if (play_err != ESP_OK) {
        ESP_LOGW(kTag, "Buzzer startup pattern failed: %s",
                 esp_err_to_name(play_err));
    }
}

void InitDisplayService()
{
    const esp_err_t err = display_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Display service init failed: %s", esp_err_to_name(err));
    }
}

void InitStorageService()
{
    const esp_err_t err = storage_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Storage service init failed: %s", esp_err_to_name(err));
    }
    storage_service::LogDebugStatus();
}

void InitTouchService()
{
    touch_service::SetEventHandler(HandleTouchEvent, nullptr);
    const esp_err_t err = touch_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Touch service init failed: %s", esp_err_to_name(err));
    }
}

}  // namespace

void Run()
{
    ESP_ERROR_CHECK(power_service::EnablePowerHold());
    ConfirmPendingOtaImage();
    ESP_ERROR_CHECK(power_service::Init());
    power_service::LogDebugStatus();
    InitBuzzerService();
    InitDisplayService();
    InitTouchService();
    InitStorageService();
    StartShutdownTask();
    InitButtonService();
}

}  // namespace app_shell
