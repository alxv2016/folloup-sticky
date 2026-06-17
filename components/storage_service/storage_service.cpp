#include "storage_service.h"

#include <cstring>
#include <vector>

#include "driver/spi_master.h"
#include "esp_log.h"
#include "sd_card.h"
#include "sticky_board_config.h"

namespace storage_service {
namespace {

constexpr const char* kTag = "StorageService";
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kProbePath = "/sdcard/sticky_sd_probe.txt";
constexpr const char* kProbeText = "reTerminal Sticky SD probe\n";
constexpr size_t kAllocationUnitSize = 16 * 1024;
constexpr size_t kMaxFiles = 5;
constexpr size_t kMaxLoggedEntries = 5;

bool s_initialized = false;
esp_err_t s_mount_result = ESP_ERR_INVALID_STATE;

SdCardPins BuildPins()
{
    SdCardPins pins = {};
    pins.host_id = SPI2_HOST;
    pins.clk = STICKY_SD_CLK_PIN;
    pins.mosi = STICKY_SD_MOSI_PIN;
    pins.miso = STICKY_SD_MISO_PIN;
    pins.cs = STICKY_SD_CS_PIN;
    pins.power_enable = STICKY_SD_POWER_EN_PIN;
    pins.power_active_level = 1;
    pins.card_detect = STICKY_SD_DETECT_PIN;
    pins.card_detect_active_level = 0;
    return pins;
}

SdCard& Card()
{
    static SdCard card(BuildPins(), kMountPoint);
    return card;
}

void LogRootDirectory(SdCard& card)
{
    std::vector<SdCardFileEntry> entries;
    const esp_err_t err = card.ListDirectoryAll(card.mount_point().c_str(), &entries);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "List root directory failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(kTag, "root directory entries=%u", static_cast<unsigned>(entries.size()));
    for (size_t i = 0; i < entries.size() && i < kMaxLoggedEntries; ++i) {
        ESP_LOGI(kTag, "entry %u: %s%s (%llu bytes)",
                 static_cast<unsigned>(i + 1),
                 entries[i].name,
                 entries[i].is_directory ? "/" : "",
                 static_cast<unsigned long long>(entries[i].size_bytes));
    }
}

void RunProbeFile(SdCard& card)
{
    const bool write_ok = card.WriteBufferToFile(
        kProbePath,
        reinterpret_cast<const uint8_t*>(kProbeText),
        std::strlen(kProbeText));
    ESP_LOGI(kTag, "probe write %s: %s", kProbePath, write_ok ? "ok" : "failed");
    if (!write_ok) {
        return;
    }

    char readback[64] = {};
    const size_t bytes_read =
        card.ReadFromOffset(kProbePath, readback, sizeof(readback) - 1, 0);
    readback[bytes_read] = '\0';
    ESP_LOGI(kTag, "probe read: %u bytes '%s'",
             static_cast<unsigned>(bytes_read), readback);
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return s_mount_result == ESP_ERR_NOT_FOUND ? ESP_OK : s_mount_result;
    }

    SdCard& card = Card();
    const bool inserted = card.IsCardInserted();
    ESP_LOGI(kTag, "SD detect state: %s", inserted ? "inserted" : "not inserted");
    if (!inserted) {
        s_initialized = true;
        s_mount_result = ESP_ERR_NOT_FOUND;
        return ESP_OK;
    }

    s_mount_result = card.Mount(false, kAllocationUnitSize, kMaxFiles, true);
    ESP_LOGI(kTag, "Mount %s: %s", kMountPoint, esp_err_to_name(s_mount_result));
    s_initialized = true;
    return s_mount_result;
}

void LogDebugStatus()
{
    SdCard& card = Card();
    ESP_LOGI(kTag, "status: initialized=%d inserted=%d mounted=%d mount_result=%s",
             s_initialized ? 1 : 0,
             card.IsCardInserted() ? 1 : 0,
             card.IsMounted() ? 1 : 0,
             esp_err_to_name(s_mount_result));

    if (!card.IsMounted()) {
        return;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    if (card.GetStorageStats(total_bytes, free_bytes)) {
        ESP_LOGI(kTag, "storage: total=%llu bytes free=%llu bytes",
                 static_cast<unsigned long long>(total_bytes),
                 static_cast<unsigned long long>(free_bytes));
    } else {
        ESP_LOGW(kTag, "Storage stats unavailable");
    }

    LogRootDirectory(card);
    RunProbeFile(card);
}

bool IsMounted()
{
    return Card().IsMounted();
}

}  // namespace storage_service
