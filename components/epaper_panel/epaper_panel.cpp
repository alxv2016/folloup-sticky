#include "epaper_panel.h"

#include <algorithm>
#include <cstring>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* kTag = "EpaperPanel";
constexpr int kSpiDmaChunkSizeBytes = 1024;
constexpr int kSpiBusMaxTransferSizeBytes = 48 * 1024;
constexpr int kBusyPollDelayMs = 5;
constexpr int kSpiClockHz = 20 * 1000 * 1000;

}  // namespace

EpaperPanel::EpaperPanel(int width, int height, const EpaperPanelConfig& config)
    : width_(width), height_(height), config_(config)
{
}

EpaperPanel::~EpaperPanel()
{
    if (spi_tx_buffer_ != nullptr) {
        heap_caps_free(spi_tx_buffer_);
    }
    if (framebuffer_ != nullptr) {
        heap_caps_free(framebuffer_);
    }
    if (previous_framebuffer_ != nullptr) {
        heap_caps_free(previous_framebuffer_);
    }
    if (spi_ != nullptr) {
        spi_bus_remove_device(spi_);
    }
    if (spi_bus_initialized_here_) {
        spi_bus_free(config_.spi_host);
    }
}

esp_err_t EpaperPanel::Initialize()
{
    if (framebuffer_ != nullptr && previous_framebuffer_ != nullptr &&
        state_ == EpaperPanelState::kActive) {
        return ESP_OK;
    }
    if (config_.buffer_len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(InitSpiPort(), kTag, "SPI init failed");
    ESP_RETURN_ON_ERROR(InitGpio(), kTag, "GPIO init failed");

    if (framebuffer_ == nullptr) {
        framebuffer_ = static_cast<uint8_t*>(
            heap_caps_malloc(config_.buffer_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (framebuffer_ == nullptr) {
            framebuffer_ =
                static_cast<uint8_t*>(heap_caps_malloc(config_.buffer_len, MALLOC_CAP_8BIT));
        }
        if (framebuffer_ == nullptr) {
            ESP_LOGE(kTag, "Failed to allocate %d-byte framebuffer", config_.buffer_len);
            return ESP_ERR_NO_MEM;
        }
    }

    if (previous_framebuffer_ == nullptr) {
        previous_framebuffer_ = static_cast<uint8_t*>(
            heap_caps_malloc(config_.buffer_len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (previous_framebuffer_ == nullptr) {
            previous_framebuffer_ =
                static_cast<uint8_t*>(heap_caps_malloc(config_.buffer_len, MALLOC_CAP_8BIT));
        }
        if (previous_framebuffer_ == nullptr) {
            ESP_LOGE(kTag, "Failed to allocate %d-byte retained framebuffer", config_.buffer_len);
            return ESP_ERR_NO_MEM;
        }
        memset(previous_framebuffer_, 0xFF, static_cast<size_t>(config_.buffer_len));
    }

    if (spi_tx_buffer_ == nullptr) {
        spi_tx_buffer_ = static_cast<uint8_t*>(
            heap_caps_malloc(kSpiDmaChunkSizeBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA |
                                                        MALLOC_CAP_8BIT));
        if (spi_tx_buffer_ == nullptr) {
            ESP_LOGE(kTag, "Failed to allocate SPI DMA staging buffer");
            return ESP_ERR_NO_MEM;
        }
        spi_tx_buffer_len_ = kSpiDmaChunkSizeBytes;
    }

    Clear(true);
    state_ = EpaperPanelState::kUninitialized;
    return ESP_OK;
}

bool EpaperPanel::RequiresBaseRefresh() const
{
    return state_ != EpaperPanelState::kActive || !base_image_initialized_;
}

bool EpaperPanel::CanPartialRefresh(int max_partial_refreshes) const
{
    return state_ == EpaperPanelState::kActive && base_image_initialized_ &&
           partial_refresh_count_ < max_partial_refreshes;
}

void EpaperPanel::ResetMetrics()
{
    metrics_ = {};
}

void EpaperPanel::Clear(bool white)
{
    if (framebuffer_ != nullptr) {
        memset(framebuffer_, white ? 0xFF : 0x00, static_cast<size_t>(config_.buffer_len));
    }
}

void EpaperPanel::SetCs(int level)
{
    gpio_set_level(config_.cs, level);
}

void EpaperPanel::SetDc(int level)
{
    gpio_set_level(config_.dc, level);
}

void EpaperPanel::SetRst(int level)
{
    gpio_set_level(config_.rst, level);
}

esp_err_t EpaperPanel::InitGpio()
{
    gpio_config_t out = {};
    out.intr_type = GPIO_INTR_DISABLE;
    out.mode = GPIO_MODE_OUTPUT;
    out.pin_bit_mask = (1ULL << config_.rst) | (1ULL << config_.dc) | (1ULL << config_.cs);
    out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&out), kTag, "configure output GPIOs failed");

    gpio_config_t busy = {};
    busy.intr_type = GPIO_INTR_DISABLE;
    busy.mode = GPIO_MODE_INPUT;
    busy.pin_bit_mask = 1ULL << config_.busy;
    busy.pull_down_en = GPIO_PULLDOWN_DISABLE;
    busy.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&busy), kTag, "configure busy GPIO failed");

    SetRst(1);
    SetDc(0);
    SetCs(1);
    return ESP_OK;
}

esp_err_t EpaperPanel::InitSpiPort()
{
    if (spi_ != nullptr) {
        return ESP_OK;
    }

    if (!config_.external_spi_bus) {
        spi_bus_config_t buscfg = {};
        buscfg.miso_io_num = config_.miso;
        buscfg.mosi_io_num = config_.mosi;
        buscfg.sclk_io_num = config_.sck;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = kSpiBusMaxTransferSizeBytes;

        esp_err_t err = spi_bus_initialize(config_.spi_host, &buscfg, SPI_DMA_CH_AUTO);
        if (err == ESP_OK) {
            spi_bus_initialized_here_ = true;
        } else if (err != ESP_ERR_INVALID_STATE) {
            return err;
        }
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.spics_io_num = -1;
    devcfg.clock_speed_hz = kSpiClockHz;
    devcfg.mode = 0;
    devcfg.queue_size = 1;
    return spi_bus_add_device(config_.spi_host, &devcfg, &spi_);
}

esp_err_t EpaperPanel::HardwareReset()
{
    const int64_t start_us = esp_timer_get_time();
    SetRst(0);
    vTaskDelay(pdMS_TO_TICKS(config_.reset_low_ms));
    SetRst(1);
    vTaskDelay(pdMS_TO_TICKS(config_.reset_high_ms));
    metrics_.reset_sequence_us = esp_timer_get_time() - start_us;
    return ESP_OK;
}

esp_err_t EpaperPanel::ReadBusy()
{
    const int64_t start_us = esp_timer_get_time();
    const int64_t timeout_us = static_cast<int64_t>(config_.busy_timeout_ms) * 1000LL;
    while (gpio_get_level(config_.busy) == static_cast<int>(config_.busy_level)) {
        if (timeout_us > 0 && (esp_timer_get_time() - start_us) >= timeout_us) {
            ESP_LOGW(kTag, "Busy wait timed out after %lu ms",
                     static_cast<unsigned long>(config_.busy_timeout_ms));
            state_ = EpaperPanelState::kUninitialized;
            base_image_initialized_ = false;
            wake_refresh_pending_ = true;
            partial_refresh_count_ = 0;
            return ESP_ERR_TIMEOUT;
        }
        // At least one tick. pdMS_TO_TICKS(5) is 0 at CONFIG_FREERTOS_HZ=100, and
        // vTaskDelay(0) does not block -- it yields only to equal-or-higher priority, so
        // the idle task (priority 0) never ran while this task (priority 3) polled. A
        // panel busy for a couple of seconds, or the 10s timeout path, then starved IDLE
        // on this core and tripped the task watchdog.
        vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(kBusyPollDelayMs)));
    }
    metrics_.panel_busy_us += esp_timer_get_time() - start_us;
    return ESP_OK;
}

esp_err_t EpaperPanel::TransmitBytes(const uint8_t* data, int len)
{
    if (data == nullptr || len <= 0) {
        return ESP_OK;
    }
    if (spi_ == nullptr || spi_tx_buffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    int remaining = len;
    int offset = 0;
    while (remaining > 0) {
        const int chunk_size = std::min(remaining, spi_tx_buffer_len_);
        memcpy(spi_tx_buffer_, data + offset, static_cast<size_t>(chunk_size));

        spi_transaction_t transaction = {};
        transaction.length = 8 * chunk_size;
        transaction.tx_buffer = spi_tx_buffer_;

        const int64_t transfer_start_us = esp_timer_get_time();
        const esp_err_t err = spi_device_polling_transmit(spi_, &transaction);
        metrics_.spi_transfer_us += esp_timer_get_time() - transfer_start_us;
        if (err != ESP_OK) {
            ESP_LOGE(kTag, "SPI transmit failed at offset %d: %s", offset, esp_err_to_name(err));
            return err;
        }
        remaining -= chunk_size;
        offset += chunk_size;
    }
    return ESP_OK;
}

esp_err_t EpaperPanel::SendCommand(uint8_t command)
{
    SetDc(0);
    SetCs(0);
    const esp_err_t err = TransmitBytes(&command, 1);
    SetCs(1);
    return err;
}

esp_err_t EpaperPanel::SendData(uint8_t data)
{
    SetDc(1);
    SetCs(0);
    const esp_err_t err = TransmitBytes(&data, 1);
    SetCs(1);
    return err;
}

esp_err_t EpaperPanel::SendCommandWithData(uint8_t command, const uint8_t* data, int len)
{
    SetCs(0);
    SetDc(0);
    esp_err_t err = TransmitBytes(&command, 1);
    if (err == ESP_OK && data != nullptr && len > 0) {
        SetDc(1);
        err = TransmitBytes(data, len);
    }
    SetCs(1);
    return err;
}

esp_err_t EpaperPanel::WriteBytes(const uint8_t* data, int len)
{
    SetDc(1);
    SetCs(0);
    const esp_err_t err = TransmitBytes(data, len);
    SetCs(1);
    return err;
}
