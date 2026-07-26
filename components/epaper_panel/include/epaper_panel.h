#ifndef EPAPER_PANEL_H_
#define EPAPER_PANEL_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

#ifndef GPIO_NUM_NC
#define GPIO_NUM_NC ((gpio_num_t)-1)
#endif

struct EpaperPanelMetrics {
    int64_t panel_busy_us = 0;
    int64_t spi_transfer_us = 0;
    int64_t reset_sequence_us = 0;
    int64_t trigger_us = 0;
    int64_t init_ready_us = 0;
};

enum class EpaperPanelState : uint8_t {
    kUninitialized,
    kActive,
    kDeepSleep,
};

struct EpaperPanelConfig {
    spi_host_device_t spi_host = SPI2_HOST;
    gpio_num_t cs = GPIO_NUM_NC;
    gpio_num_t dc = GPIO_NUM_NC;
    gpio_num_t rst = GPIO_NUM_NC;
    gpio_num_t busy = GPIO_NUM_NC;
    gpio_num_t mosi = GPIO_NUM_NC;
    gpio_num_t miso = GPIO_NUM_NC;
    gpio_num_t sck = GPIO_NUM_NC;
    bool external_spi_bus = false;
    int buffer_len = 0;
    uint32_t busy_timeout_ms = 10000;
    uint16_t reset_low_ms = 2;
    uint16_t reset_high_ms = 50;
    uint8_t busy_level = 1;
};

class EpaperPanel {
public:
    EpaperPanel(int width, int height, const EpaperPanelConfig& config);
    ~EpaperPanel();

    EpaperPanel(const EpaperPanel&) = delete;
    EpaperPanel& operator=(const EpaperPanel&) = delete;

    esp_err_t Initialize();
    esp_err_t RefreshFull();
    esp_err_t RefreshFullBase();
    // Full-screen redraw on the panel's fast OTP waveform: quicker than
    // RefreshFullBase, but clears accumulated ghosting less thoroughly.
    esp_err_t RefreshFastBase();
    esp_err_t RefreshChangedRegion();
    esp_err_t RefreshPartialFullScreen();
    esp_err_t Sleep();
    void Clear(bool white = true);
    void ResetMetrics();

    uint8_t* framebuffer() { return framebuffer_; }
    const EpaperPanelMetrics& metrics() const { return metrics_; }
    EpaperPanelState state() const { return state_; }
    bool base_image_initialized() const { return base_image_initialized_; }
    bool wake_refresh_pending() const { return wake_refresh_pending_; }
    int partial_refresh_count() const { return partial_refresh_count_; }
    bool RequiresBaseRefresh() const;
    bool CanPartialRefresh(int max_partial_refreshes) const;

private:
    esp_err_t InitSpiPort();
    esp_err_t InitGpio();
    esp_err_t HardwareReset();
    esp_err_t ReadBusy();
    esp_err_t TransmitBytes(const uint8_t* data, int len);
    esp_err_t SendCommand(uint8_t command);
    esp_err_t SendData(uint8_t data);
    esp_err_t SendCommandWithData(uint8_t command, const uint8_t* data, int len);
    esp_err_t WriteBytes(const uint8_t* data, int len);
    esp_err_t SetWindow(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
    esp_err_t SetCursor(uint16_t x_start, uint16_t y_start);
    esp_err_t RefreshFullBaseInternal(bool fast);
    esp_err_t InitFull(bool fast);
    esp_err_t InitPartial();
    esp_err_t DisplayFullBase();
    esp_err_t TurnOnDisplay();
    esp_err_t TurnOnDisplayFast();
    esp_err_t TurnOnDisplayPart();
    void CopyFramebufferToPrevious();
    void SetCs(int level);
    void SetDc(int level);
    void SetRst(int level);

    const int width_;
    const int height_;
    const EpaperPanelConfig config_;
    spi_device_handle_t spi_ = nullptr;
    bool spi_bus_initialized_here_ = false;
    uint8_t* framebuffer_ = nullptr;
    uint8_t* previous_framebuffer_ = nullptr;
    uint8_t* spi_tx_buffer_ = nullptr;
    int spi_tx_buffer_len_ = 0;
    EpaperPanelState state_ = EpaperPanelState::kUninitialized;
    bool base_image_initialized_ = false;
    bool wake_refresh_pending_ = false;
    int partial_refresh_count_ = 0;
    EpaperPanelMetrics metrics_ = {};
};

#endif  // EPAPER_PANEL_H_
