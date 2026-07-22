#ifndef BOARD_DRIVERS_ES8311_CODEC_H_
#define BOARD_DRIVERS_ES8311_CODEC_H_

#include "audio_codec.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>
#include <mutex>

class Es8311Codec : public AudioCodec {
private:
    const audio_codec_data_if_t* data_if_ = nullptr;
    const audio_codec_ctrl_if_t* ctrl_if_ = nullptr;
    const audio_codec_if_t* codec_if_ = nullptr;
    const audio_codec_gpio_if_t* gpio_if_ = nullptr;

    esp_codec_dev_handle_t dev_ = nullptr;
    gpio_num_t pa_pin_ = GPIO_NUM_NC;
    bool pa_inverted_ = false;
    bool channels_enabled_ = false;
    std::mutex data_if_mutex_;

    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws, gpio_num_t dout,
                              gpio_num_t din);
    void SetChannelsEnabled(bool enabled);
    void UpdatePaState();
    void UpdateDeviceState();

    int Read(int16_t* dest, int samples) override;
    int Write(const int16_t* data, int samples) override;

public:
    Es8311Codec(void* i2c_master_handle, i2c_port_t i2c_port, int input_sample_rate,
                     int output_sample_rate, gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws,
                     gpio_num_t dout, gpio_num_t din, gpio_num_t pa_pin, uint8_t es8311_addr,
                     bool use_mclk = true, bool pa_inverted = false);
    ~Es8311Codec() override;

    void SetInputGain(float gain) override;
    void SetOutputVolume(int volume) override;
    void SetOutputMuted(bool muted) override;
    void EnableInput(bool enable) override;
    void EnableOutput(bool enable) override;
    void Shutdown() override;
};

#endif  // BOARD_DRIVERS_ES8311_CODEC_H_
