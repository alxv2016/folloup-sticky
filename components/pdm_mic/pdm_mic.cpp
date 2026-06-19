#include "pdm_mic.h"

#include "driver/gpio.h"
#include "driver/i2s_pdm.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace pdm_mic {
namespace {

constexpr const char* kTag = "PdmMic";
constexpr uint32_t kPowerSettleDelayMs = 10;

}  // namespace

PdmMic::PdmMic(const Config& config) : config_(config)
{
}

PdmMic::~PdmMic()
{
    Shutdown();
}

esp_err_t PdmMic::ConfigurePowerPin()
{
    if (config_.power_pin == GPIO_NUM_NC) {
        return ESP_OK;
    }

    gpio_config_t power_cfg = {};
    power_cfg.pin_bit_mask = 1ULL << config_.power_pin;
    power_cfg.mode = GPIO_MODE_OUTPUT;
    power_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    power_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_cfg.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&power_cfg);
    if (err != ESP_OK) {
        return err;
    }

    return SetPowerEnabled(false);
}

esp_err_t PdmMic::SetPowerEnabled(bool enabled)
{
    if (config_.power_pin == GPIO_NUM_NC) {
        return ESP_OK;
    }

    const int level = enabled ? static_cast<int>(config_.power_active_level)
                              : static_cast<int>(!config_.power_active_level);
    return gpio_set_level(config_.power_pin, level);
}

esp_err_t PdmMic::Init()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return ESP_OK;
    }
    if (config_.clk_pin == GPIO_NUM_NC || config_.data_pin == GPIO_NUM_NC ||
        config_.sample_rate_hz <= 0 || config_.dma_desc_num == 0 ||
        config_.dma_frame_num == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ConfigurePowerPin();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Mic power pin init failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = config_.dma_desc_num;
    chan_cfg.dma_frame_num = config_.dma_frame_num;
    err = i2s_new_channel(&chan_cfg, nullptr, &rx_handle_);
    if (err != ESP_OK) {
        rx_handle_ = nullptr;
        ESP_LOGW(kTag, "I2S RX channel create failed: %s", esp_err_to_name(err));
        return err;
    }

    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(
            static_cast<uint32_t>(config_.sample_rate_hz)),
        .slot_cfg = I2S_PDM_RX_SLOT_PCM_FMT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                           I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = config_.clk_pin,
            .din = config_.data_pin,
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    err = i2s_channel_init_pdm_rx_mode(rx_handle_, &pdm_rx_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "I2S PDM RX init failed: %s", esp_err_to_name(err));
        i2s_del_channel(rx_handle_);
        rx_handle_ = nullptr;
        return err;
    }

    initialized_ = true;
    ESP_LOGI(kTag, "PDM RX initialized: sample_rate=%d clk=GPIO%d data=GPIO%d power=GPIO%d",
             config_.sample_rate_hz,
             static_cast<int>(config_.clk_pin),
             static_cast<int>(config_.data_pin),
             static_cast<int>(config_.power_pin));
    return ESP_OK;
}

esp_err_t PdmMic::Enable()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || rx_handle_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (enabled_) {
        return ESP_OK;
    }

    esp_err_t err = SetPowerEnabled(true);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(kPowerSettleDelayMs));

    err = i2s_channel_enable(rx_handle_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        SetPowerEnabled(false);
        ESP_LOGW(kTag, "I2S PDM RX enable failed: %s", esp_err_to_name(err));
        return err;
    }

    enabled_ = true;
    return ESP_OK;
}

esp_err_t PdmMic::Disable()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_ || rx_handle_ == nullptr) {
        SetPowerEnabled(false);
        enabled_ = false;
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;
    if (enabled_) {
        err = i2s_channel_disable(rx_handle_);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "I2S PDM RX disable failed: %s", esp_err_to_name(err));
        }
    }

    enabled_ = false;
    const esp_err_t power_err = SetPowerEnabled(false);
    return err == ESP_OK ? power_err : err;
}

esp_err_t PdmMic::Read(int16_t* dest, size_t sample_count, size_t* samples_read,
                       uint32_t timeout_ms)
{
    if (samples_read != nullptr) {
        *samples_read = 0;
    }
    if (dest == nullptr || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_ || rx_handle_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_read = 0;
    esp_err_t err = i2s_channel_read(rx_handle_, dest, sample_count * sizeof(int16_t),
                                     &bytes_read, timeout_ms);
    if (err != ESP_OK) {
        return err;
    }

    if (samples_read != nullptr) {
        *samples_read = bytes_read / sizeof(int16_t);
    }
    return ESP_OK;
}

void PdmMic::Shutdown()
{
    Disable();

    std::lock_guard<std::mutex> lock(mutex_);
    if (rx_handle_ != nullptr) {
        const esp_err_t err = i2s_del_channel(rx_handle_);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "I2S RX channel delete failed: %s", esp_err_to_name(err));
        }
        rx_handle_ = nullptr;
    }
    initialized_ = false;
}

}  // namespace pdm_mic
