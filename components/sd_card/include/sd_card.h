#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"
#include "esp_err.h"

constexpr size_t kSdCardMaxEntryNameLength = 255;

struct SdCardFileEntry {
    char name[kSdCardMaxEntryNameLength] = {};
    bool is_directory = false;
    uint64_t size_bytes = 0;
};

struct SdCardPins {
    spi_host_device_t host_id = SPI2_HOST;
    gpio_num_t clk = GPIO_NUM_NC;
    gpio_num_t mosi = GPIO_NUM_NC;
    gpio_num_t miso = GPIO_NUM_NC;
    gpio_num_t cs = GPIO_NUM_NC;
    bool external_spi_bus = false;
    gpio_num_t power_enable = GPIO_NUM_NC;
    int power_active_level = 1;
    gpio_num_t card_detect = GPIO_NUM_NC;
    int card_detect_active_level = 0;
};

class SdCard {
public:
    explicit SdCard(const SdCardPins& pins, std::string mount_point = "/sdcard");
    ~SdCard();

    esp_err_t Mount(bool format_if_mount_failed = false, size_t allocation_unit_size = 32 * 1024,
                    size_t max_files = 5, bool log_failures = false);
    esp_err_t Format();
    esp_err_t SetVolumeLabel(const char* volume_label);
    void Unmount();
    bool IsMounted() const;
    bool IsCardInserted() const;

    esp_err_t ListDirectoryOnce(const char* path, SdCardFileEntry* entries, int* count,
                                int max_entries) const;
    esp_err_t ListDirectoryPage(const char* path, SdCardFileEntry* entries, int start_index,
                                int page_size, int* count) const;
    esp_err_t ListDirectoryAll(const char* path, std::vector<SdCardFileEntry>* entries) const;
    int GetDirectoryEntryCount(const char* path) const;
    int GetDirectoryFileCount(const char* path) const;
    bool GetFileInfo(const char* path, bool* exists, bool* is_directory,
                     uint64_t* size_bytes) const;

    size_t ReadFromOffset(const char* path, char* buffer, size_t length, size_t offset) const;
    size_t AppendToFile(const char* path, const char* buffer, size_t length) const;
    bool TruncateFile(const char* path) const;
    bool ReadFileToBuffer(const char* path, uint8_t* buffer, size_t buffer_size) const;
    bool WriteBufferToFile(const char* path, const uint8_t* buffer, size_t buffer_size) const;
    bool GetStorageStats(uint64_t& total_bytes, uint64_t& free_bytes) const;

    const std::string& mount_point() const { return mount_point_; }

private:
    void ConfigurePowerPin() const;
    void ConfigureDetectPin() const;
    void SetPowerEnabled(bool enabled) const;

    SdCardPins pins_;
    std::string mount_point_;
    sdmmc_card_t* card_ = nullptr;
    size_t last_allocation_unit_size_ = 32 * 1024;
    size_t last_max_files_ = 5;

    static int CompareEntries(const void* lhs, const void* rhs);
};
