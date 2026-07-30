#include "waveshare_board.h"

#include <memory>

#include "waveshare_board_config.h"

#include "axp2101.h"
#include "board_es8311_codec.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace waveshare_board {
namespace {

constexpr const char* kTag = "WaveshareBoard";

i2c_master_bus_handle_t s_sensor_i2c_bus = nullptr;
std::unique_ptr<Axp2101> s_pmic;
std::unique_ptr<Es8311Codec> s_audio_codec;

esp_err_t CreateI2cBus(i2c_port_num_t port, gpio_num_t scl_pin, gpio_num_t sda_pin,
                       i2c_master_bus_handle_t* out_bus)
{
    if (out_bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t config = {};
    config.i2c_port = port;
    config.scl_io_num = scl_pin;
    config.sda_io_num = sda_pin;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = WAVESHARE_I2C_GLITCH_IGNORE_CNT;
    config.flags.enable_internal_pullup = 1;

    return i2c_new_master_bus(&config, out_bus);
}

// Bring the AXP2101 rails and charger to the Waveshare board's operating profile.
// DC1 + ALDO1..3 all sit at 3.3 V (system, e-paper, audio, and SD/sensor rails);
// the charger targets a single-cell 4.2 V pack at 400 mA CC.
void ConfigurePmicRails(Axp2101* pmic)
{
    pmic->setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);
    pmic->setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_900MA);
    pmic->setSysPowerDownVoltage(2800);

    pmic->enableDC1();
    pmic->setDC1Voltage(3300);
    pmic->enableALDO1();
    pmic->setALDO1Voltage(3300);
    pmic->enableALDO2();
    pmic->setALDO2Voltage(3300);
    pmic->enableALDO3();
    pmic->setALDO3Voltage(3300);

    pmic->setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);
    pmic->setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_75MA);
    pmic->setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_400MA);
    pmic->setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    pmic->enableButtonBatteryCharge();

    // Hardware power key. Three behaviors layered on one physical key:
    //   - 1s hold from off powers the board on.
    //   - Short press and >=1s press each raise a distinct IRQ that the firmware owns
    //     (lock-screen toggle and shutdown confirmation respectively).
    //   - A sustained 6s hold lets the PMIC hard-cut the rails, so there is always a
    //     hardware escape even if the firmware is wedged.
    // IrqLevelTime is what separates the short IRQ from the long one, so it has to sit
    // well below the 6s hardware cut for the software path to get a chance.
    pmic->SetPowerKeyPressOffTime(Axp2101::PowerKeyPressOffTime::k6S);
    pmic->SetPowerKeyPressOnTime(Axp2101::PowerKeyPressOnTime::k1S);
    pmic->SetIrqLevelTime(Axp2101::IrqLevelTime::k1S);
    pmic->SetButtonPowerOffEnabled(true);

    // Power-key IRQs only. VBUS insert/remove is deliberately excluded: nothing consumes
    // those events, and the PMIC IRQ line is a light-sleep wake source, so enabling them
    // would wake the board every time USB is plugged or unplugged.
    pmic->EnablePowerKeyIrq(false);
    pmic->ClearIrqStatus();
}

}  // namespace

esp_err_t EnablePowerHold()
{
    if (s_pmic != nullptr) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = EnsureSensorI2cBus(&bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "PMIC I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    // The AXP2101 base ctor talks to the chip and aborts if it is absent; on this
    // board it feeds every rail, so a missing PMIC is unrecoverable by design.
    s_pmic = std::make_unique<Axp2101>(bus, WAVESHARE_AXP2101_I2C_ADDR, WAVESHARE_PMIC_IRQ_PIN);
    ConfigurePmicRails(s_pmic.get());

    ESP_LOGI(kTag, "AXP2101 power hold established (batt=%d%% vbus=%d charging=%d)",
             s_pmic->GetBatteryLevel(), s_pmic->isVbusIn() ? 1 : 0,
             s_pmic->IsCharging() ? 1 : 0);
    return ESP_OK;
}

Axp2101* GetPmic()
{
    return s_pmic.get();
}

AudioCodec* GetAudioCodec()
{
    if (s_audio_codec != nullptr) {
        return s_audio_codec.get();
    }

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = EnsureSensorI2cBus(&bus);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Audio codec I2C bus init failed: %s", esp_err_to_name(err));
        return nullptr;
    }

    // ES8311 control shares the sensor I2C bus; audio streams over I2S0. Full-duplex
    // at a single 16 kHz clock (input == output) drives both capture and playback.
    s_audio_codec = std::make_unique<Es8311Codec>(
        bus, WAVESHARE_SENSOR_I2C_PORT,
        WAVESHARE_AUDIO_SAMPLE_RATE_HZ, WAVESHARE_AUDIO_SAMPLE_RATE_HZ,
        WAVESHARE_AUDIO_I2S_MCLK, WAVESHARE_AUDIO_I2S_BCLK, WAVESHARE_AUDIO_I2S_WS,
        WAVESHARE_AUDIO_I2S_DOUT, WAVESHARE_AUDIO_I2S_DIN, WAVESHARE_AUDIO_PA_PIN,
        ES8311_CODEC_DEFAULT_ADDR);
    // Keep the output path (and PA) enabled for the codec's lifetime so system
    // sound cues and clip playback can write to it without per-event PA toggling
    // (input is enabled on demand by the recording service).
    s_audio_codec->EnableOutput(true);
    ESP_LOGI(kTag, "ES8311 audio codec created (%d Hz duplex, output enabled)",
             WAVESHARE_AUDIO_SAMPLE_RATE_HZ);
    return s_audio_codec.get();
}

esp_err_t EnsureSensorI2cBus(i2c_master_bus_handle_t* out_bus)
{
    if (out_bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_sensor_i2c_bus != nullptr) {
        *out_bus = s_sensor_i2c_bus;
        return ESP_OK;
    }

    esp_err_t err = CreateI2cBus(WAVESHARE_SENSOR_I2C_PORT, WAVESHARE_SENSOR_I2C_SCL_PIN,
                                 WAVESHARE_SENSOR_I2C_SDA_PIN, &s_sensor_i2c_bus);
    if (err != ESP_OK) {
        s_sensor_i2c_bus = nullptr;
        return err;
    }

    *out_bus = s_sensor_i2c_bus;
    ESP_LOGI(kTag, "Sensor I2C bus initialized: port=%d scl=GPIO%d sda=GPIO%d",
             static_cast<int>(WAVESHARE_SENSOR_I2C_PORT),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SCL_PIN),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SDA_PIN));
    return ESP_OK;
}

esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus)
{
    return EnsureSensorI2cBus(out_bus);
}

esp_err_t AddPcf85063Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device)
{
    if (bus == nullptr || out_device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = WAVESHARE_PCF85063_I2C_ADDR;
    config.scl_speed_hz = WAVESHARE_I2C_SPEED_HZ;

    return i2c_master_bus_add_device(bus, &config, out_device);
}

}  // namespace waveshare_board
