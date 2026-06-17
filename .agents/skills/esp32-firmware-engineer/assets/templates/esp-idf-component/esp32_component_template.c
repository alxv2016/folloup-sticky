#include "esp32_component_template.h"

#include <inttypes.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "comp_template";

static TaskHandle_t s_task;
static esp32_component_template_config_t s_cfg;
static bool s_initialized;

static void component_task(void *arg)
{
    const TickType_t period = pdMS_TO_TICKS(s_cfg.period_ms);
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        int level = s_cfg.active_high ? 1 : 0;
        gpio_set_level(s_cfg.gpio_num, level);
        ESP_LOGD(TAG, "tick gpio=%d level=%d", s_cfg.gpio_num, level);
        vTaskDelayUntil(&last, period);
    }
}

esp_err_t esp32_component_template_init(const esp32_component_template_config_t *cfg)
{
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, TAG, "cfg is null");
    ESP_RETURN_ON_FALSE(cfg->period_ms > 0, ESP_ERR_INVALID_ARG, TAG, "period_ms=0");
    ESP_RETURN_ON_FALSE(GPIO_IS_VALID_OUTPUT_GPIO(cfg->gpio_num), ESP_ERR_INVALID_ARG, TAG,
                        "invalid output gpio=%d", cfg->gpio_num);

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << cfg->gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "gpio_config failed");

    s_cfg = *cfg;
    s_initialized = true;
    return ESP_OK;
}

esp_err_t esp32_component_template_start(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(s_task == NULL, ESP_ERR_INVALID_STATE, TAG, "already started");

    BaseType_t ok = xTaskCreate(component_task, "comp_tmpl", 3072, NULL, tskIDLE_PRIORITY + 1, &s_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "xTaskCreate failed");
    ESP_LOGI(TAG, "started gpio=%d period_ms=%" PRIu32, s_cfg.gpio_num, s_cfg.period_ms);
    return ESP_OK;
}

esp_err_t esp32_component_template_stop(void)
{
    ESP_RETURN_ON_FALSE(s_task != NULL, ESP_ERR_INVALID_STATE, TAG, "not started");
    vTaskDelete(s_task);
    s_task = NULL;
    ESP_LOGI(TAG, "stopped");
    return ESP_OK;
}
