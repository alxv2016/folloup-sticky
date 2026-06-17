#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int gpio_num;
    uint32_t period_ms;
    bool active_high;
} esp32_component_template_config_t;

esp_err_t esp32_component_template_init(const esp32_component_template_config_t *cfg);
esp_err_t esp32_component_template_start(void);
esp_err_t esp32_component_template_stop(void);

#ifdef __cplusplus
}
#endif
