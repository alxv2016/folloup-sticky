#include "app_shell.h"

#include "button_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "power_service.h"

namespace app_shell {
namespace {

constexpr const char* kTag = "AppShell";

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

void InitButtonService()
{
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
    InitButtonService();
}

}  // namespace app_shell
