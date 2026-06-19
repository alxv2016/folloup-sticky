#ifndef PDM_MIC_H_
#define PDM_MIC_H_

#include <cstddef>
#include <cstdint>
#include <mutex>

#include "driver/gpio.h"
#include "driver/i2s_common.h"
#include "esp_err.h"

namespace pdm_mic {

struct Config {
    int sample_rate_hz = 16000;
    gpio_num_t clk_pin = GPIO_NUM_NC;
    gpio_num_t data_pin = GPIO_NUM_NC;
    gpio_num_t power_pin = GPIO_NUM_NC;
    bool power_active_level = true;
    uint32_t dma_desc_num = 6;
    uint32_t dma_frame_num = 240;
};

class PdmMic {
public:
    explicit PdmMic(const Config& config);
    ~PdmMic();

    esp_err_t Init();
    esp_err_t Enable();
    esp_err_t Disable();
    esp_err_t Read(int16_t* dest, size_t sample_count, size_t* samples_read,
                   uint32_t timeout_ms);
    void Shutdown();

    bool initialized() const { return initialized_; }
    bool enabled() const { return enabled_; }
    int sample_rate_hz() const { return config_.sample_rate_hz; }

private:
    esp_err_t ConfigurePowerPin();
    esp_err_t SetPowerEnabled(bool enabled);

    Config config_;
    i2s_chan_handle_t rx_handle_ = nullptr;
    bool initialized_ = false;
    bool enabled_ = false;
    std::mutex mutex_;
};

}  // namespace pdm_mic

#endif  // PDM_MIC_H_
