#include "sd_card.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <utility>

#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sdkconfig.h"
#include "esp_timer.h"

namespace {

constexpr const char* kTag = "SdCard";
constexpr size_t kMaxTransferSize = 48 * 1024;
constexpr const char* kSdmmcCommonTag = "sdmmc_common";
constexpr const char* kVfsFatSdmmcTag = "vfs_fat_sdmmc";

class ScopedLogSilencer {
public:
    ScopedLogSilencer(const char* tag1, const char* tag2)
        : tag1_(tag1),
          tag2_(tag2),
          tag1_level_(esp_log_level_get(tag1)),
          tag2_level_(esp_log_level_get(tag2))
    {
        esp_log_level_set(tag1_, ESP_LOG_NONE);
        esp_log_level_set(tag2_, ESP_LOG_NONE);
    }

    ~ScopedLogSilencer()
    {
        esp_log_level_set(tag1_, tag1_level_);
        esp_log_level_set(tag2_, tag2_level_);
    }

private:
    const char* tag1_;
    const char* tag2_;
    esp_log_level_t tag1_level_;
    esp_log_level_t tag2_level_;
};

std::string BuildEntryPath(const char* directory_path, const char* entry_name)
{
    if (directory_path == nullptr || entry_name == nullptr) {
        return {};
    }

    std::string path = directory_path;
    if (!path.empty() && path.back() != '/') {
        path.push_back('/');
    }
    path += entry_name;
    return path;
}

void PopulateEntryMetadata(const char* directory_path, const struct dirent* dir_entry,
                           SdCardFileEntry* entry)
{
    if (dir_entry == nullptr || entry == nullptr) {
        return;
    }

    strlcpy(entry->name, dir_entry->d_name, sizeof(entry->name));
    entry->is_directory = dir_entry->d_type == DT_DIR;
    entry->size_bytes = 0;

    const std::string full_path = BuildEntryPath(directory_path, dir_entry->d_name);
    struct stat st = {};
    if (!full_path.empty() && stat(full_path.c_str(), &st) == 0) {
        entry->is_directory = S_ISDIR(st.st_mode);
        if (!entry->is_directory && st.st_size > 0) {
            entry->size_bytes = static_cast<uint64_t>(st.st_size);
        }
    }
}

}  // namespace

SdCard::SdCard(const SdCardPins& pins, std::string mount_point)
    : pins_(pins), mount_point_(std::move(mount_point))
{
}

SdCard::~SdCard()
{
    Unmount();
}

void SdCard::ConfigurePowerPin() const
{
    if (pins_.power_enable == GPIO_NUM_NC) {
        return;
    }

    gpio_config_t power_cfg = {};
    power_cfg.pin_bit_mask = 1ULL << pins_.power_enable;
    power_cfg.mode = GPIO_MODE_OUTPUT;
    power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&power_cfg);
}

void SdCard::ConfigureDetectPin() const
{
    if (pins_.card_detect == GPIO_NUM_NC) {
        return;
    }

    gpio_config_t detect_cfg = {};
    detect_cfg.pin_bit_mask = 1ULL << pins_.card_detect;
    detect_cfg.mode = GPIO_MODE_INPUT;
    detect_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    detect_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    detect_cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&detect_cfg);
}

void SdCard::SetPowerEnabled(bool enabled) const
{
    if (pins_.power_enable == GPIO_NUM_NC) {
        return;
    }

    const int level = enabled ? pins_.power_active_level : !pins_.power_active_level;
    gpio_set_level(pins_.power_enable, level);
}

esp_err_t SdCard::Mount(bool format_if_mount_failed, size_t allocation_unit_size,
                        size_t max_files, bool log_failures)
{
    if (IsMounted()) {
        return ESP_OK;
    }

    ConfigurePowerPin();
    ConfigureDetectPin();
    SetPowerEnabled(true);

    if (!IsCardInserted()) {
        if (log_failures) {
            ESP_LOGW(kTag, "no SD card detected");
        }
        SetPowerEnabled(false);
        return ESP_ERR_NOT_FOUND;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = format_if_mount_failed,
        .max_files = static_cast<int>(max_files),
        .allocation_unit_size = allocation_unit_size,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = pins_.host_id;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;

    if (!pins_.external_spi_bus) {
        spi_bus_config_t bus_config = {};
        bus_config.mosi_io_num = pins_.mosi;
        bus_config.miso_io_num = pins_.miso;
        bus_config.sclk_io_num = pins_.clk;
        bus_config.quadwp_io_num = -1;
        bus_config.quadhd_io_num = -1;
        bus_config.max_transfer_sz = kMaxTransferSize;

        esp_err_t err = spi_bus_initialize(pins_.host_id, &bus_config, SDSPI_DEFAULT_DMA);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            if (log_failures) {
                ESP_LOGW(kTag, "failed to initialize SPI bus for SD card: %s",
                         esp_err_to_name(err));
            }
            SetPowerEnabled(false);
            return err;
        }
    }

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = pins_.host_id;
    slot_config.gpio_cs = pins_.cs;
    slot_config.gpio_cd = pins_.card_detect;

    ScopedLogSilencer silence_sdmmc_logs(kSdmmcCommonTag, kVfsFatSdmmcTag);
    esp_err_t err = esp_vfs_fat_sdspi_mount(mount_point_.c_str(), &host, &slot_config,
                                            &mount_config, &card_);
    if (err != ESP_OK) {
        if (log_failures) {
            ESP_LOGW(kTag, "failed to mount SD card at %s: %s", mount_point_.c_str(),
                     esp_err_to_name(err));
        }
        card_ = nullptr;
        SetPowerEnabled(false);
        return err;
    }

    last_allocation_unit_size_ = allocation_unit_size;
    last_max_files_ = max_files;
    sdmmc_card_print_info(stdout, card_);
    return ESP_OK;
}

esp_err_t SdCard::Format()
{
    if (!IsMounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    const int64_t started_at_us = esp_timer_get_time();
    sdmmc_card_t* formatted_card = card_;
    ESP_LOGI(kTag,
             "Format begin: mount_point=%s card=%p inserted=%d mounted=%d",
             mount_point_.c_str(),
             static_cast<void*>(formatted_card),
             IsCardInserted() ? 1 : 0,
             IsMounted() ? 1 : 0);
    esp_err_t err = esp_vfs_fat_sdcard_format(mount_point_.c_str(), formatted_card);
    ESP_LOGI(kTag,
             "Format FAT call returned: err=%s elapsed_ms=%lld",
             esp_err_to_name(err),
             static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to format SD card at %s: %s", mount_point_.c_str(),
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "Format step: unmount after FAT format");
    Unmount();
    ESP_LOGI(kTag,
             "Format step: unmount complete inserted=%d mounted=%d elapsed_ms=%lld",
             IsCardInserted() ? 1 : 0,
             IsMounted() ? 1 : 0,
             static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
    ESP_LOGI(kTag, "Format step: remount after FAT format");
    err = Mount(false, last_allocation_unit_size_, last_max_files_, true);
    ESP_LOGI(kTag,
             "Format step: remount complete err=%s inserted=%d mounted=%d elapsed_ms=%lld",
             esp_err_to_name(err),
             IsCardInserted() ? 1 : 0,
             IsMounted() ? 1 : 0,
             static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "failed to remount SD card after format at %s: %s",
                 mount_point_.c_str(), esp_err_to_name(err));
    }
    return err;
}

esp_err_t SdCard::SetVolumeLabel(const char* volume_label)
{
    if (!IsMounted() || volume_label == nullptr || volume_label[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

#if CONFIG_FATFS_USE_LABEL
    FRESULT result = f_setlabel(volume_label);
    if (result != FR_OK) {
        ESP_LOGW(kTag, "failed to set SD card volume label to %s: FatFS error %d", volume_label,
                 static_cast<int>(result));
        return ESP_FAIL;
    }
    return ESP_OK;
#else
    ESP_LOGW(kTag, "FATFS volume labels are disabled; skipping label %s", volume_label);
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

void SdCard::Unmount()
{
    if (!IsMounted()) {
        SetPowerEnabled(false);
        return;
    }
    esp_vfs_fat_sdcard_unmount(mount_point_.c_str(), card_);
    card_ = nullptr;
    SetPowerEnabled(false);
}

bool SdCard::IsMounted() const
{
    return card_ != nullptr;
}

bool SdCard::IsCardInserted() const
{
    if (pins_.card_detect == GPIO_NUM_NC) {
        return true;
    }
    ConfigureDetectPin();
    return gpio_get_level(pins_.card_detect) == pins_.card_detect_active_level;
}

esp_err_t SdCard::ListDirectoryOnce(const char* path, SdCardFileEntry* entries, int* count,
                                    int max_entries) const
{
    if (path == nullptr || entries == nullptr || count == nullptr || max_entries <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        ESP_LOGE(kTag, "failed to open directory: %s", path);
        *count = 0;
        return ESP_ERR_NOT_FOUND;
    }

    struct dirent* entry = nullptr;
    int found = 0;
    while ((entry = readdir(dir)) != nullptr && found < max_entries) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        PopulateEntryMetadata(path, entry, &entries[found]);
        ++found;
    }
    closedir(dir);

    if (found > 1) {
        qsort(entries, found, sizeof(SdCardFileEntry), CompareEntries);
    }

    *count = found;
    return ESP_OK;
}

esp_err_t SdCard::ListDirectoryPage(const char* path, SdCardFileEntry* entries, int start_index,
                                    int page_size, int* count) const
{
    if (path == nullptr || entries == nullptr || count == nullptr || start_index < 0 ||
        page_size <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        ESP_LOGE(kTag, "failed to open directory: %s", path);
        *count = 0;
        return ESP_ERR_NOT_FOUND;
    }

    struct dirent* entry = nullptr;
    int skipped = 0;
    int found = 0;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (skipped < start_index) {
            ++skipped;
            continue;
        }
        if (found >= page_size) {
            break;
        }

        PopulateEntryMetadata(path, entry, &entries[found]);
        ++found;
    }
    closedir(dir);

    if (found > 1) {
        qsort(entries, found, sizeof(SdCardFileEntry), CompareEntries);
    }

    *count = found;
    return ESP_OK;
}

esp_err_t SdCard::ListDirectoryAll(const char* path, std::vector<SdCardFileEntry>* entries) const
{
    if (path == nullptr || entries == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    entries->clear();
    if (!IsMounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        ESP_LOGE(kTag, "failed to open directory for full list: %s errno=%d", path, errno);
        return ESP_ERR_NOT_FOUND;
    }

    struct dirent* dir_entry = nullptr;
    while ((dir_entry = readdir(dir)) != nullptr) {
        if (strcmp(dir_entry->d_name, ".") == 0 || strcmp(dir_entry->d_name, "..") == 0) {
            continue;
        }

        SdCardFileEntry entry = {};
        PopulateEntryMetadata(path, dir_entry, &entry);
        entries->push_back(std::move(entry));
    }
    closedir(dir);

    if (entries->size() > 1) {
        qsort(entries->data(), entries->size(), sizeof(SdCardFileEntry), CompareEntries);
    }

    ESP_LOGI(kTag, "listed directory path=%s entries=%u", path,
             static_cast<unsigned>(entries->size()));
    return ESP_OK;
}

int SdCard::GetDirectoryEntryCount(const char* path) const
{
    if (path == nullptr) {
        return 0;
    }

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        return 0;
    }

    struct dirent* entry = nullptr;
    int count = 0;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            ++count;
        }
    }
    closedir(dir);
    return count;
}

int SdCard::GetDirectoryFileCount(const char* path) const
{
    if (path == nullptr) {
        return 0;
    }

    DIR* dir = opendir(path);
    if (dir == nullptr) {
        return 0;
    }

    struct dirent* entry = nullptr;
    int count = 0;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        SdCardFileEntry metadata = {};
        PopulateEntryMetadata(path, entry, &metadata);
        if (!metadata.is_directory) {
            ++count;
        }
    }
    closedir(dir);
    return count;
}

bool SdCard::GetFileInfo(const char* path, bool* exists, bool* is_directory,
                         uint64_t* size_bytes) const
{
    if (exists != nullptr) {
        *exists = false;
    }
    if (is_directory != nullptr) {
        *is_directory = false;
    }
    if (size_bytes != nullptr) {
        *size_bytes = 0;
    }
    if (path == nullptr || !IsMounted()) {
        return false;
    }

    struct stat st = {};
    if (stat(path, &st) != 0) {
        return false;
    }

    if (exists != nullptr) {
        *exists = true;
    }
    if (is_directory != nullptr) {
        *is_directory = S_ISDIR(st.st_mode);
    }
    if (size_bytes != nullptr && !S_ISDIR(st.st_mode) && st.st_size > 0) {
        *size_bytes = static_cast<uint64_t>(st.st_size);
    }
    return true;
}

size_t SdCard::ReadFromOffset(const char* path, char* buffer, size_t length, size_t offset) const
{
    if (path == nullptr || buffer == nullptr || length == 0 || !IsMounted()) {
        return 0;
    }

    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "failed to open file for read: %s", path);
        return 0;
    }

    fseek(file, static_cast<long>(offset), SEEK_SET);
    size_t bytes_read = fread(buffer, 1, length, file);
    fclose(file);
    return bytes_read;
}

size_t SdCard::AppendToFile(const char* path, const char* buffer, size_t length) const
{
    if (path == nullptr || buffer == nullptr || length == 0 || !IsMounted()) {
        return 0;
    }

    errno = 0;
    FILE* file = fopen(path, "ab");
    if (file == nullptr) {
        ESP_LOGE(kTag, "failed to open file for append: %s errno=%d (%s)",
                 path, errno, strerror(errno));
        return 0;
    }

    size_t bytes_written = fwrite(buffer, 1, length, file);
    fclose(file);
    return bytes_written;
}

bool SdCard::TruncateFile(const char* path) const
{
    if (path == nullptr || !IsMounted()) {
        return false;
    }

    errno = 0;
    FILE* file = fopen(path, "wb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "failed to truncate file: %s errno=%d (%s)",
                 path, errno, strerror(errno));
        return false;
    }
    fclose(file);
    return true;
}

bool SdCard::ReadFileToBuffer(const char* path, uint8_t* buffer, size_t buffer_size) const
{
    if (path == nullptr || buffer == nullptr || buffer_size == 0 || !IsMounted()) {
        return false;
    }

    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "failed to open file for read: %s", path);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    size_t to_read =
        file_size > 0 && static_cast<size_t>(file_size) < buffer_size
            ? static_cast<size_t>(file_size)
            : buffer_size;
    size_t bytes_read = fread(buffer, 1, to_read, file);
    fclose(file);

    if (bytes_read < buffer_size) {
        memset(buffer + bytes_read, 0xFF, buffer_size - bytes_read);
    }
    return true;
}

bool SdCard::WriteBufferToFile(const char* path, const uint8_t* buffer, size_t buffer_size) const
{
    if (path == nullptr || buffer == nullptr || buffer_size == 0 || !IsMounted()) {
        return false;
    }

    errno = 0;
    FILE* file = fopen(path, "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "failed to open file for write: %s errno=%d (%s)",
                 path, errno, strerror(errno));
        return false;
    }

    size_t bytes_written = fwrite(buffer, 1, buffer_size, file);
    if (bytes_written != buffer_size) {
        ESP_LOGW(kTag, "short write: %s wrote=%u expected=%u errno=%d (%s)",
                 path, static_cast<unsigned>(bytes_written),
                 static_cast<unsigned>(buffer_size), errno, strerror(errno));
    }
    fclose(file);
    return bytes_written == buffer_size;
}

bool SdCard::GetStorageStats(uint64_t& total_bytes, uint64_t& free_bytes) const
{
    total_bytes = 0;
    free_bytes = 0;
    if (!IsMounted()) {
        return false;
    }

    if (esp_vfs_fat_info(mount_point_.c_str(), &total_bytes, &free_bytes) != ESP_OK) {
        ESP_LOGW(kTag, "failed to read storage stats for %s", mount_point_.c_str());
        return false;
    }
    return total_bytes > 0;
}

int SdCard::CompareEntries(const void* lhs, const void* rhs)
{
    const auto* left = static_cast<const SdCardFileEntry*>(lhs);
    const auto* right = static_cast<const SdCardFileEntry*>(rhs);
    if (left->is_directory != right->is_directory) {
        return right->is_directory - left->is_directory;
    }
    return strcasecmp(left->name, right->name);
}
