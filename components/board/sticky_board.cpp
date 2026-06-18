#include "sticky_board.h"

#include "sticky_board_config.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace sticky_board {
namespace {

constexpr const char* kTag = "StickyBoard";
constexpr TickType_t kPowerLatchSettleDelay = pdMS_TO_TICKS(20);
constexpr TickType_t kPowerLatchShutdownHoldDelay = pdMS_TO_TICKS(500);
constexpr adc_atten_t kPowerSenseAttenuation = ADC_ATTEN_DB_12;
constexpr adc_bitwidth_t kPowerSenseBitWidth = ADC_BITWIDTH_DEFAULT;
constexpr int kPowerSenseSampleCount = 8;

adc_oneshot_unit_handle_t s_power_adc_handle = nullptr;
adc_cali_handle_t s_power_adc_cali_handle = nullptr;
adc_unit_t s_power_adc_unit = ADC_UNIT_1;
adc_channel_t s_power_adc_channel = ADC_CHANNEL_0;
bool s_power_adc_calibrated = false;
bool s_shared_spi_bus_initialized = false;

esp_err_t EnableOutputPin(gpio_num_t pin, int level)
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << pin;
    config.mode = GPIO_MODE_INPUT_OUTPUT;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK) {
        return err;
    }
    return gpio_set_level(pin, level);
}

esp_err_t PreparePowerLatchPins(const char* phase)
{
    gpio_deep_sleep_hold_dis();

    esp_err_t err = gpio_hold_dis(STICKY_POWER_HOLD_PIN);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "%s gpio_hold_dis(GPIO%d) failed: %s", phase,
                 STICKY_POWER_HOLD_PIN, esp_err_to_name(err));
        return err;
    }
    err = gpio_hold_dis(STICKY_POWER_LOCK_PIN);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "%s gpio_hold_dis(GPIO%d) failed: %s", phase,
                 STICKY_POWER_LOCK_PIN, esp_err_to_name(err));
        return err;
    }

    err = gpio_reset_pin(STICKY_POWER_HOLD_PIN);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "%s gpio_reset_pin(GPIO%d) failed: %s", phase,
                 STICKY_POWER_HOLD_PIN, esp_err_to_name(err));
        return err;
    }
    err = gpio_reset_pin(STICKY_POWER_LOCK_PIN);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "%s gpio_reset_pin(GPIO%d) failed: %s", phase,
                 STICKY_POWER_LOCK_PIN, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "%s latch GPIO holds disabled and pads reset", phase);
    return ESP_OK;
}

void LogPowerLatchLevels(const char* phase)
{
    ESP_LOGI(kTag, "%s PWR_HOLD(GPIO%d)=%d PWR_LOCK(GPIO%d)=%d", phase,
             STICKY_POWER_HOLD_PIN, gpio_get_level(STICKY_POWER_HOLD_PIN),
             STICKY_POWER_LOCK_PIN, gpio_get_level(STICKY_POWER_LOCK_PIN));
}

bool CreateAdcCalibration(adc_unit_t unit, adc_channel_t channel,
                          adc_cali_handle_t* out_handle)
{
    if (out_handle == nullptr) {
        return false;
    }

    *out_handle = nullptr;
    esp_err_t err = ESP_ERR_NOT_SUPPORTED;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = unit,
        .chan = channel,
        .atten = kPowerSenseAttenuation,
        .bitwidth = kPowerSenseBitWidth,
    };
    err = adc_cali_create_scheme_curve_fitting(&cali_config, out_handle);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = unit,
        .atten = kPowerSenseAttenuation,
        .bitwidth = kPowerSenseBitWidth,
    };
    err = adc_cali_create_scheme_line_fitting(&cali_config, out_handle);
#endif

    if (err == ESP_OK) {
        return true;
    }
    if (err != ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(kTag, "ADC calibration init failed: %s", esp_err_to_name(err));
    }
    return false;
}

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
    config.glitch_ignore_cnt = STICKY_I2C_GLITCH_IGNORE_CNT;
    config.flags.enable_internal_pullup = 1;

    return i2c_new_master_bus(&config, out_bus);
}

}  // namespace

esp_err_t EnablePowerHold()
{
    esp_err_t err = PreparePowerLatchPins("Power latch enable prepare:");
    if (err != ESP_OK) {
        return err;
    }

    err = EnableOutputPin(STICKY_POWER_HOLD_PIN, 1);
    if (err != ESP_OK) {
        return err;
    }
    err = EnableOutputPin(STICKY_POWER_LOCK_PIN, 1);
    if (err != ESP_OK) {
        return err;
    }

    ESP_LOGI(kTag, "Power hold enabled on GPIO%d/GPIO%d", STICKY_POWER_HOLD_PIN,
             STICKY_POWER_LOCK_PIN);
    LogPowerLatchLevels("Power latch enabled:");
    return ESP_OK;
}

esp_err_t ReleasePowerHold()
{
    esp_err_t err = PreparePowerLatchPins("Power latch release prepare:");
    if (err != ESP_OK) {
        return err;
    }

    LogPowerLatchLevels("Power latch release start:");

    // Page 6 tracing shows U3 Q driving Q7, and Q7 can pull on the PWR_HOLD
    // node that also gates Q2. First latch U3 Q low while PWR_HOLD is low so
    // Q7 releases, then drive PWR_HOLD high to try to turn Q2 off.
    err = EnableOutputPin(STICKY_POWER_LOCK_PIN, 0);
    if (err != ESP_OK) {
        return err;
    }
    LogPowerLatchLevels("Power latch release lock-low:");
    vTaskDelay(kPowerLatchSettleDelay);

    err = EnableOutputPin(STICKY_POWER_HOLD_PIN, 0);
    if (err != ESP_OK) {
        return err;
    }
    LogPowerLatchLevels("Power latch release hold-low:");
    vTaskDelay(kPowerLatchSettleDelay);

    err = EnableOutputPin(STICKY_POWER_LOCK_PIN, 1);
    if (err != ESP_OK) {
        return err;
    }
    LogPowerLatchLevels("Power latch release lock-pulse-high:");
    vTaskDelay(kPowerLatchSettleDelay);

    err = EnableOutputPin(STICKY_POWER_LOCK_PIN, 0);
    if (err != ESP_OK) {
        return err;
    }
    LogPowerLatchLevels("Power latch release lock-low-after-pulse:");
    vTaskDelay(kPowerLatchSettleDelay);

    err = EnableOutputPin(STICKY_POWER_HOLD_PIN, 1);
    if (err != ESP_OK) {
        return err;
    }
    LogPowerLatchLevels("Power latch release hold-high:");
    vTaskDelay(kPowerLatchSettleDelay);

    vTaskDelay(kPowerLatchShutdownHoldDelay);
    LogPowerLatchLevels("Power latch release complete:");

    ESP_LOGI(kTag, "Power hold released on GPIO%d/GPIO%d", STICKY_POWER_HOLD_PIN,
             STICKY_POWER_LOCK_PIN);
    return ESP_OK;
}

esp_err_t RestorePowerHold()
{
    ESP_LOGW(kTag, "Reasserting power hold");
    return EnablePowerHold();
}

esp_err_t ConfigureChargerPins()
{
    gpio_config_t charge_enable_config = {};
    charge_enable_config.pin_bit_mask = 1ULL << STICKY_BATTERY_CHARGE_EN_PIN;
    charge_enable_config.mode = GPIO_MODE_OUTPUT;
    charge_enable_config.pull_up_en = GPIO_PULLUP_DISABLE;
    charge_enable_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charge_enable_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t err = gpio_config(&charge_enable_config);
    if (err != ESP_OK) {
        return err;
    }

    gpio_config_t charge_state_config = {};
    charge_state_config.pin_bit_mask = 1ULL << STICKY_CHARGE_STATE_PIN;
    charge_state_config.mode = GPIO_MODE_INPUT;
    charge_state_config.pull_up_en = GPIO_PULLUP_ENABLE;
    charge_state_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charge_state_config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&charge_state_config);
}

esp_err_t SetChargerEnabled(bool enabled)
{
    return gpio_set_level(STICKY_BATTERY_CHARGE_EN_PIN, enabled ? 0 : 1);
}

esp_err_t ReadChargeState(bool* charging)
{
    if (charging == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *charging = gpio_get_level(STICKY_CHARGE_STATE_PIN) == 0;
    return ESP_OK;
}

esp_err_t InitPowerInputSense()
{
    if (s_power_adc_handle != nullptr) {
        return ESP_OK;
    }

    esp_err_t err = adc_oneshot_io_to_channel(STICKY_POWER_INPUT_VOLT_PIN,
                                              &s_power_adc_unit,
                                              &s_power_adc_channel);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "GPIO%d is not a valid ADC input: %s",
                 STICKY_POWER_INPUT_VOLT_PIN, esp_err_to_name(err));
        return err;
    }

    adc_oneshot_unit_init_cfg_t unit_config = {};
    unit_config.unit_id = s_power_adc_unit;
    err = adc_oneshot_new_unit(&unit_config, &s_power_adc_handle);
    if (err != ESP_OK) {
        s_power_adc_handle = nullptr;
        return err;
    }

    adc_oneshot_chan_cfg_t channel_config = {};
    channel_config.atten = kPowerSenseAttenuation;
    channel_config.bitwidth = kPowerSenseBitWidth;
    err = adc_oneshot_config_channel(s_power_adc_handle, s_power_adc_channel,
                                     &channel_config);
    if (err != ESP_OK) {
        adc_oneshot_del_unit(s_power_adc_handle);
        s_power_adc_handle = nullptr;
        return err;
    }

    s_power_adc_calibrated = CreateAdcCalibration(s_power_adc_unit,
                                                  s_power_adc_channel,
                                                  &s_power_adc_cali_handle);
    if (!s_power_adc_calibrated) {
        s_power_adc_cali_handle = nullptr;
        ESP_LOGW(kTag, "Power-input ADC calibration unavailable");
    }

    return ESP_OK;
}

esp_err_t ReadPowerInputSample(PowerInputSample* out_sample)
{
    if (out_sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_power_adc_handle == nullptr || !s_power_adc_calibrated ||
        s_power_adc_cali_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    int raw_sum = 0;
    int raw_min = 0;
    int raw_max = 0;
    int valid_samples = 0;
    for (int i = 0; i < kPowerSenseSampleCount; ++i) {
        int raw = 0;
        esp_err_t err = adc_oneshot_read(s_power_adc_handle, s_power_adc_channel, &raw);
        if (err != ESP_OK) {
            return err;
        }

        if (valid_samples == 0) {
            raw_min = raw;
            raw_max = raw;
        } else {
            if (raw < raw_min) {
                raw_min = raw;
            }
            if (raw > raw_max) {
                raw_max = raw;
            }
        }
        raw_sum += raw;
        ++valid_samples;
    }

    const int raw_average = raw_sum / valid_samples;
    int calibrated_mv = 0;
    esp_err_t err = adc_cali_raw_to_voltage(s_power_adc_cali_handle,
                                            raw_average, &calibrated_mv);
    if (err != ESP_OK) {
        return err;
    }

    out_sample->raw_average = raw_average;
    out_sample->raw_min = raw_min;
    out_sample->raw_max = raw_max;
    out_sample->calibrated_mv = calibrated_mv;
    out_sample->sample_count = valid_samples;
    return ESP_OK;
}

esp_err_t ConfigureBq27220InterruptPin()
{
    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << STICKY_BQ27220_INT_PIN;
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    return gpio_config(&config);
}

esp_err_t ReadBq27220InterruptLevel(int* level)
{
    if (level == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    *level = gpio_get_level(STICKY_BQ27220_INT_PIN);
    return ESP_OK;
}

esp_err_t EnsureSharedSpiBus()
{
    if (s_shared_spi_bus_initialized) {
        return ESP_OK;
    }

    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = STICKY_SHARED_SPI_MOSI_PIN;
    bus_config.miso_io_num = STICKY_SHARED_SPI_MISO_PIN;
    bus_config.sclk_io_num = STICKY_SHARED_SPI_CLK_PIN;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = STICKY_SHARED_SPI_MAX_TRANSFER_SIZE;

    esp_err_t err = spi_bus_initialize(STICKY_SHARED_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Shared SPI bus already initialized on host %d",
                 static_cast<int>(STICKY_SHARED_SPI_HOST));
        s_shared_spi_bus_initialized = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Shared SPI bus init failed on host %d: %s",
                 static_cast<int>(STICKY_SHARED_SPI_HOST), esp_err_to_name(err));
        return err;
    }

    s_shared_spi_bus_initialized = true;
    ESP_LOGI(kTag, "Shared SPI bus initialized: host=%d sck=GPIO%d mosi=GPIO%d miso=GPIO%d",
             static_cast<int>(STICKY_SHARED_SPI_HOST),
             static_cast<int>(STICKY_SHARED_SPI_CLK_PIN),
             static_cast<int>(STICKY_SHARED_SPI_MOSI_PIN),
             static_cast<int>(STICKY_SHARED_SPI_MISO_PIN));
    return ESP_OK;
}

esp_err_t EnableEpaperPower()
{
    esp_err_t err = EnableOutputPin(STICKY_EPD_POWER_EN_PIN, 1);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(STICKY_EPD_POWER_DELAY_MS));
    ESP_LOGI(kTag, "E-paper power enabled on GPIO%d", STICKY_EPD_POWER_EN_PIN);
    return ESP_OK;
}

esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus)
{
    // GPIO0 is also an ESP32-S3 strapping pin, so call this after startup
    // levels are no longer part of the boot-mode decision.
    return CreateI2cBus(STICKY_SENSOR_I2C_PORT, STICKY_SENSOR_I2C_SCL_PIN,
                        STICKY_SENSOR_I2C_SDA_PIN, out_bus);
}

esp_err_t AddBq27220Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device)
{
    if (bus == nullptr || out_device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = STICKY_BQ27220_I2C_ADDR;
    config.scl_speed_hz = STICKY_I2C_SPEED_HZ;

    return i2c_master_bus_add_device(bus, &config, out_device);
}

esp_err_t AddPcf8563Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device)
{
    if (bus == nullptr || out_device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = STICKY_PCF8563_I2C_ADDR;
    config.scl_speed_hz = STICKY_I2C_SPEED_HZ;

    return i2c_master_bus_add_device(bus, &config, out_device);
}

}  // namespace sticky_board
