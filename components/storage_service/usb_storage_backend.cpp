#include "usb_storage_backend.h"

#include <cstdlib>
#include <mutex>

#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "waveshare_board_config.h"

namespace usb_storage_backend {
namespace {

constexpr const char* kTag = "UsbStorageBackend";

std::mutex s_mutex;
sdmmc_card_t* s_card = nullptr;
tinyusb_msc_storage_handle_t s_storage_handle = nullptr;
bool s_driver_installed = false;
bool s_active = false;

void DeinitHost(const sdmmc_host_t& host)
{
    if (host.flags & SDMMC_HOST_FLAG_DEINIT_ARG) {
        host.deinit_p(host.slot);
        return;
    }
    host.deinit();
}

// Brings up the card directly on the SDMMC host, bypassing FATFS. storage_service has
// already unmounted the app's view by the time this runs.
esp_err_t InitializeCardLocked()
{
    if (s_card != nullptr) {
        return ESP_OK;
    }

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 4;
    slot_config.clk = WAVESHARE_SD_CLK_PIN;
    slot_config.cmd = WAVESHARE_SD_CMD_PIN;
    slot_config.d0 = WAVESHARE_SD_D0_PIN;
    slot_config.d1 = WAVESHARE_SD_D1_PIN;
    slot_config.d2 = WAVESHARE_SD_D2_PIN;
    slot_config.d3 = WAVESHARE_SD_D3_PIN;

    auto* card = static_cast<sdmmc_card_t*>(std::malloc(sizeof(sdmmc_card_t)));
    if (card == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = host.init();
    if (err != ESP_OK) {
        std::free(card);
        return err;
    }

    err = sdmmc_host_init_slot(host.slot, &slot_config);
    if (err != ESP_OK) {
        DeinitHost(host);
        std::free(card);
        return err;
    }

    err = sdmmc_card_init(&host, card);
    if (err != ESP_OK) {
        DeinitHost(host);
        std::free(card);
        return err;
    }

    s_card = card;
    return ESP_OK;
}

// Idempotent teardown, also used as the failure unwind for a partial enter.
void CleanupLocked()
{
    if (s_storage_handle != nullptr) {
        const esp_err_t err = tinyusb_msc_delete_storage(s_storage_handle);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Delete MSC storage failed: %s", esp_err_to_name(err));
        }
        s_storage_handle = nullptr;
    }

    if (s_driver_installed) {
        const esp_err_t err = tinyusb_driver_uninstall();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Uninstall TinyUSB driver failed: %s", esp_err_to_name(err));
        }
        s_driver_installed = false;
    }

    if (s_card != nullptr) {
        DeinitHost(s_card->host);
        std::free(s_card);
        s_card = nullptr;
    }

    s_active = false;
}

}  // namespace

esp_err_t EnterUsbMode()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_active) {
        return ESP_OK;
    }

    // Start from a known-clean state: a previous failed enter may have left the host or
    // the driver half-configured.
    CleanupLocked();

    esp_err_t err = InitializeCardLocked();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "SD init for USB mode failed: %s", esp_err_to_name(err));
        CleanupLocked();
        return err;
    }

    tinyusb_msc_storage_config_t storage_cfg = {};
    storage_cfg.mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB;
    storage_cfg.medium.card = s_card;
    storage_cfg.fat_fs.base_path = nullptr;
    storage_cfg.fat_fs.config.max_files = 5;
    storage_cfg.fat_fs.format_flags = 0;
    // Never format the user's card on the way into OTG. This defaults to false, which
    // lets TinyUSB format media it cannot read a filesystem from -- a transient read
    // failure would silently wipe every recording. Fail the enter instead; the caller
    // remounts app-side and surfaces the error modal.
    storage_cfg.fat_fs.do_not_format = true;
    err = tinyusb_msc_new_storage_sdmmc(&storage_cfg, &s_storage_handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Create MSC storage failed: %s", esp_err_to_name(err));
        CleanupLocked();
        return err;
    }

    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();
    err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Install TinyUSB driver failed: %s", esp_err_to_name(err));
        CleanupLocked();
        return err;
    }

    s_driver_installed = true;
    s_active = true;
    ESP_LOGI(kTag, "USB MSC mode active");
    return ESP_OK;
}

esp_err_t ExitUsbMode()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    CleanupLocked();
    ESP_LOGI(kTag, "USB MSC mode stopped");
    return ESP_OK;
}

bool IsActive()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_active;
}

}  // namespace usb_storage_backend
