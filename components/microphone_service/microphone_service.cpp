#include "microphone_service.h"

#include <algorithm>
#include <mutex>

#include "esp_log.h"
#include "pdm_mic.h"
#include "sticky_board_config.h"

namespace microphone_service {
namespace {

constexpr const char* kTag = "MicrophoneService";

pdm_mic::Config BuildMicConfig()
{
    pdm_mic::Config config = {};
    config.sample_rate_hz = static_cast<int>(kSampleRateHz);
    config.clk_pin = STICKY_PDM_CLK_PIN;
    config.data_pin = STICKY_PDM_DATA_PIN;
    config.power_pin = STICKY_PDM_POWER_EN_PIN;
    config.power_active_level = STICKY_PDM_POWER_ACTIVE_LEVEL != 0;
    return config;
}

pdm_mic::PdmMic& Mic()
{
    static pdm_mic::PdmMic mic(BuildMicConfig());
    return mic;
}

std::mutex s_mutex;
bool s_initialized = false;
uint8_t s_last_input_level_percent = 0;

}  // namespace

esp_err_t Init()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_initialized) {
            return ESP_OK;
        }
    }

    const esp_err_t err = Mic().Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "PDM mic init failed: %s", esp_err_to_name(err));
        return err;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_initialized = true;
        s_last_input_level_percent = 0;
    }

    ESP_LOGI(kTag,
             "Microphone service initialized: sample_rate=%lu clk=GPIO%d data=GPIO%d power=GPIO%d",
             static_cast<unsigned long>(kSampleRateHz),
             static_cast<int>(STICKY_PDM_CLK_PIN),
             static_cast<int>(STICKY_PDM_DATA_PIN),
             static_cast<int>(STICKY_PDM_POWER_EN_PIN));
    return ESP_OK;
}

bool IsInitialized()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_initialized;
}

esp_err_t Enable()
{
    esp_err_t err = Init();
    if (err != ESP_OK) {
        return err;
    }
    return Mic().Enable();
}

esp_err_t Disable()
{
    return Mic().Disable();
}

uint8_t CalculateLevelPercent(const int16_t* samples, size_t sample_count)
{
    if (samples == nullptr || sample_count == 0) {
        return 0;
    }

    uint64_t abs_sum = 0;
    for (size_t i = 0; i < sample_count; ++i) {
        const int32_t sample = samples[i];
        abs_sum += static_cast<uint32_t>(sample < 0 ? -sample : sample);
    }

    const uint32_t avg_abs = static_cast<uint32_t>(abs_sum / sample_count);
    return static_cast<uint8_t>(std::min<uint32_t>((avg_abs * 100U) / 32768U, 100U));
}

esp_err_t ReadSamples(int16_t* dest, size_t sample_count, size_t* samples_read,
                      uint32_t timeout_ms)
{
    size_t local_samples_read = 0;
    esp_err_t err = Mic().Read(dest, sample_count, &local_samples_read, timeout_ms);
    if (samples_read != nullptr) {
        *samples_read = local_samples_read;
    }
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t level = CalculateLevelPercent(dest, local_samples_read);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_last_input_level_percent = level;
    }
    return ESP_OK;
}

uint8_t LastInputLevelPercent()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_last_input_level_percent;
}

void LogDebugStatus()
{
    ESP_LOGI(kTag, "status: initialized=%d level=%u",
             IsInitialized() ? 1 : 0,
             static_cast<unsigned>(LastInputLevelPercent()));
}

}  // namespace microphone_service
