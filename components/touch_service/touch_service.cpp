#include "touch_service.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gt911.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace touch_service {
namespace {

constexpr const char* kTag = "TouchService";
constexpr uint32_t kTouchTaskStackWords = 4096;
constexpr UBaseType_t kTouchTaskPriority = 5;
constexpr TickType_t kTouchIdlePollDelay = pdMS_TO_TICKS(1000);

i2c_master_bus_handle_t s_touch_bus = nullptr;
TaskHandle_t s_touch_task = nullptr;
bool s_initialized = false;
EventHandler s_event_handler = nullptr;
void* s_event_handler_context = nullptr;

GT911& TouchController()
{
    static GT911 gt911;
    return gt911;
}

void DispatchTouchPoints(int8_t count, const GTPoint* points)
{
    if (count <= 0 || points == nullptr) {
        return;
    }

    TouchEventInfo event = {};
    event.count = static_cast<uint8_t>(count > kMaxTouchPoints ? kMaxTouchPoints : count);
    for (uint8_t i = 0; i < event.count; ++i) {
        event.points[i].x = points[i].x;
        event.points[i].y = points[i].y;
        event.points[i].size = points[i].size;
        event.points[i].id = points[i].id;
        ESP_LOGD(kTag, "Touch point[%u]: x=%u y=%u size=%u id=%u",
                 static_cast<unsigned>(i), static_cast<unsigned>(event.points[i].x),
                 static_cast<unsigned>(event.points[i].y),
                 static_cast<unsigned>(event.points[i].size),
                 static_cast<unsigned>(event.points[i].id));
    }

    if (s_event_handler != nullptr) {
        s_event_handler(event, s_event_handler_context);
    }
}

void IRAM_ATTR TouchInterruptIsr(void*)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (s_touch_task != nullptr) {
        vTaskNotifyGiveFromISR(s_touch_task, &higher_priority_task_woken);
    }
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t EnsureGpioIsrService()
{
    const esp_err_t err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGI(kTag, "GPIO ISR service already installed");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "GPIO ISR service install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "GPIO ISR service installed");
    return ESP_OK;
}

void TouchTask(void*)
{
    ESP_LOGI(kTag, "Touch worker started");

    while (true) {
        const uint32_t notifications =
            ulTaskNotifyTake(pdTRUE, kTouchIdlePollDelay);

        int int_level = 1;
        const esp_err_t level_err = sticky_board::ReadTouchInterruptLevel(&int_level);
        if (level_err != ESP_OK) {
            ESP_LOGW(kTag, "Read TOUCH_INT level failed: %s", esp_err_to_name(level_err));
            continue;
        }

        if (notifications == 0 && int_level != 0) {
            continue;
        }

        ESP_LOGD(kTag, "Servicing touch controller: notifications=%lu int_level=%d",
                 static_cast<unsigned long>(notifications), int_level);
        TouchController().loop();
    }
}

esp_err_t StartTouchTask()
{
    if (s_touch_task != nullptr) {
        return ESP_OK;
    }

    const BaseType_t created = xTaskCreate(TouchTask, "touch_service",
                                          kTouchTaskStackWords, nullptr,
                                          kTouchTaskPriority, &s_touch_task);
    if (created != pdPASS) {
        s_touch_task = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t AttachTouchInterrupt()
{
    ESP_RETURN_ON_ERROR(EnsureGpioIsrService(), kTag, "GPIO ISR setup failed");
    ESP_RETURN_ON_ERROR(sticky_board::ConfigureTouchInterruptPin(GPIO_INTR_NEGEDGE),
                        kTag, "TOUCH_INT configure failed");

    esp_err_t err = gpio_isr_handler_add(STICKY_TOUCH_INT_PIN, TouchInterruptIsr, nullptr);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "TOUCH_INT ISR handler already attached");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(err, kTag, "TOUCH_INT ISR handler attach failed");

    int int_level = 1;
    err = sticky_board::ReadTouchInterruptLevel(&int_level);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "TOUCH_INT level read after attach failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(kTag, "TOUCH_INT ISR attached on GPIO%d, initial level=%d",
                 STICKY_TOUCH_INT_PIN, int_level);
    }
    return ESP_OK;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(kTag,
             "Initializing touch: power=GPIO%d int=GPIO%d rst=GPIO%d scl=GPIO%d sda=GPIO%d map=%ux%u",
             STICKY_TOUCH_POWER_EN_PIN, STICKY_TOUCH_INT_PIN, STICKY_TOUCH_RST_PIN,
             STICKY_TOUCH_I2C_SCL_PIN, STICKY_TOUCH_I2C_SDA_PIN,
             static_cast<unsigned>(STICKY_TOUCH_LOGICAL_WIDTH),
             static_cast<unsigned>(STICKY_TOUCH_LOGICAL_HEIGHT));

    ESP_RETURN_ON_ERROR(sticky_board::EnableTouchPower(), kTag,
                        "touch power enable failed");
    ESP_RETURN_ON_ERROR(sticky_board::CreateTouchI2cBus(&s_touch_bus), kTag,
                        "touch I2C bus init failed");
    ESP_LOGI(kTag, "Touch I2C bus initialized on port %d",
             static_cast<int>(STICKY_TOUCH_I2C_PORT));

    GT911& gt911 = TouchController();
    if (!gt911.begin(STICKY_TOUCH_INT_PIN, STICKY_TOUCH_RST_PIN,
                     STICKY_TOUCH_LOGICAL_WIDTH, STICKY_TOUCH_LOGICAL_HEIGHT,
                     s_touch_bus)) {
        return ESP_FAIL;
    }

    uint16_t max_x = 0;
    uint16_t max_y = 0;
    gt911.readResolution(max_x, max_y);
    ESP_LOGI(kTag, "GT911 ready: addr=0x%02X sensor=%ux%u logical=%ux%u",
             gt911.address(), static_cast<unsigned>(max_x),
             static_cast<unsigned>(max_y),
             static_cast<unsigned>(STICKY_TOUCH_LOGICAL_WIDTH),
             static_cast<unsigned>(STICKY_TOUCH_LOGICAL_HEIGHT));

    gt911.onTouch(DispatchTouchPoints);

    ESP_RETURN_ON_ERROR(StartTouchTask(), kTag, "touch task start failed");
    ESP_RETURN_ON_ERROR(AttachTouchInterrupt(), kTag, "touch interrupt attach failed");

    s_initialized = true;
    ESP_LOGI(kTag, "Touch service initialized");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

void SetEventHandler(EventHandler handler, void* context)
{
    s_event_handler = handler;
    s_event_handler_context = context;
}

}  // namespace touch_service
