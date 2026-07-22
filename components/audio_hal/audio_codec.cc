#include "audio_codec.h"

#include <algorithm>

#include <driver/i2s_common.h>
#include <esp_log.h>

#define TAG "AudioCodec"

AudioCodec::AudioCodec() {}

AudioCodec::~AudioCodec() {}

bool AudioCodec::OutputData(std::vector<int16_t>& data) {
    return Write(data.data(), data.size()) == static_cast<int>(data.size());
}

bool AudioCodec::OutputData(const int16_t* data, size_t samples) {
    return data != nullptr && Write(data, static_cast<int>(samples)) == static_cast<int>(samples);
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    return Read(data.data(), data.size()) > 0;
}

int AudioCodec::ReadInputSamples(std::vector<int16_t>& data) {
    return Read(data.data(), data.size());
}

void AudioCodec::Start() {
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)",
                 output_volume_);
        output_volume_ = 10;
    }

    ESP_LOGI(TAG, "Audio codec started");
}

void AudioCodec::SetOutputVolume(int volume) {
    int clamped_volume = std::clamp(volume, 0, 100);
    if (clamped_volume != volume) {
        ESP_LOGW(TAG, "Clamped output volume from %d to %d", volume, clamped_volume);
    }
    output_volume_ = clamped_volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
}

void AudioCodec::SetOutputMuted(bool muted) {
    output_muted_ = muted;
    ESP_LOGI(TAG, "Set output mute to %s", muted ? "true" : "false");
}

void AudioCodec::SetInputGain(float gain) {
    input_gain_ = gain;
    ESP_LOGI(TAG, "Set input gain to %.1f", input_gain_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}

void AudioCodec::Shutdown() {
    SetOutputMuted(true);
    EnableOutput(false);
    EnableInput(false);
    ESP_LOGI(TAG, "Audio codec shut down");
}
