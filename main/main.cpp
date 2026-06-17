#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

namespace {
void confirm_pending_ota_image()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;

    if (running != nullptr &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}
}  // namespace

extern "C" void app_main(void)
{
    confirm_pending_ota_image();
}
