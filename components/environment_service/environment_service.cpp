#include "environment_service.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sht40.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace environment_service {
namespace {

constexpr const char* kTag = "EnvironmentService";
constexpr int kDebugSampleCount = 3;
constexpr TickType_t kDebugSampleDelay = pdMS_TO_TICKS(250);

i2c_master_bus_handle_t s_sensor_bus = nullptr;
i2c_master_dev_handle_t s_sht40_device = nullptr;
uint8_t s_active_address = 0;
uint32_t s_serial_number = 0;
bool s_initialized = false;

void RemoveDevice()
{
    if (s_sht40_device != nullptr) {
        const esp_err_t err = i2c_master_bus_rm_device(s_sht40_device);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Remove SHT40 I2C device failed: %s", esp_err_to_name(err));
        }
        s_sht40_device = nullptr;
    }
}

void RecoverSensorBus(const char* reason)
{
    if (s_sensor_bus == nullptr) {
        return;
    }

    const esp_err_t err = i2c_master_bus_reset(s_sensor_bus);
    ESP_LOGW(kTag, "Sensor I2C bus reset after %s: %s", reason, esp_err_to_name(err));
}

esp_err_t TryAddress(uint8_t address)
{
    RemoveDevice();

    esp_err_t err = sticky_board::AddSht40Device(s_sensor_bus, address, &s_sht40_device);
    ESP_LOGI(kTag, "Add SHT40 I2C device addr=0x%02X: %s",
             address, esp_err_to_name(err));
    if (err != ESP_OK) {
        s_sht40_device = nullptr;
        return err;
    }

    uint32_t serial_number = 0;
    err = sht40::read_serial_number(s_sht40_device, serial_number);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHT40 serial read failed at addr=0x%02X: %s",
                 address, esp_err_to_name(err));
        RecoverSensorBus("SHT40 serial read failure");
        RemoveDevice();
        return err;
    }

    s_active_address = address;
    s_serial_number = serial_number;
    ESP_LOGI(kTag, "SHT40 ready: addr=0x%02X serial=0x%08lX",
             s_active_address, static_cast<unsigned long>(s_serial_number));
    return ESP_OK;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(kTag,
             "Initializing SHT40: primary=0x%02X alternate=0x%02X bus=%d scl=GPIO%d sda=GPIO%d",
             STICKY_SHT40_I2C_ADDR, STICKY_SHT40_ALT_I2C_ADDR,
             static_cast<int>(STICKY_SENSOR_I2C_PORT),
             static_cast<int>(STICKY_SENSOR_I2C_SCL_PIN),
             static_cast<int>(STICKY_SENSOR_I2C_SDA_PIN));

    esp_err_t err = sticky_board::EnsureSensorI2cBus(&s_sensor_bus);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Sensor I2C bus unavailable: %s", esp_err_to_name(err));
        return err;
    }

    err = TryAddress(STICKY_SHT40_I2C_ADDR);
    if (err != ESP_OK && STICKY_SHT40_ALT_I2C_ADDR != STICKY_SHT40_I2C_ADDR) {
        ESP_LOGW(kTag, "Retrying SHT40 at alternate addr=0x%02X",
                 STICKY_SHT40_ALT_I2C_ADDR);
        err = TryAddress(STICKY_SHT40_ALT_I2C_ADDR);
    }

    if (err != ESP_OK) {
        s_active_address = 0;
        s_serial_number = 0;
        ESP_LOGW(kTag, "SHT40 init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(kTag, "Environment service initialized");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

esp_err_t ReadSample(EnvironmentSample* out_sample)
{
    if (out_sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_sht40_device == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    sht40::Measurement measurement = {};
    const esp_err_t err = sht40::read_measurement(s_sht40_device, measurement,
                                                  sht40::Precision::kHigh);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "SHT40 measurement read failed: %s", esp_err_to_name(err));
        RecoverSensorBus("SHT40 measurement read failure");
        return err;
    }

    EnvironmentSample sample = {};
    sample.temperature_c = measurement.temperature_c;
    sample.humidity_percent = measurement.humidity_percent;
    sample.address = s_active_address;
    sample.serial_number = s_serial_number;
    *out_sample = sample;
    return ESP_OK;
}

void LogDebugStatus()
{
    if (!s_initialized) {
        ESP_LOGW(kTag, "Environment sensor unavailable");
        return;
    }

    for (int i = 0; i < kDebugSampleCount; ++i) {
        vTaskDelay(kDebugSampleDelay);

        EnvironmentSample sample = {};
        const esp_err_t err = ReadSample(&sample);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Sample %d failed: %s", i + 1, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(kTag,
                 "Sample %d addr=0x%02X serial=0x%08lX temp=%.2fC humidity=%.2f%%",
                 i + 1,
                 sample.address,
                 static_cast<unsigned long>(sample.serial_number),
                 static_cast<double>(sample.temperature_c),
                 static_cast<double>(sample.humidity_percent));
    }
}

}  // namespace environment_service
