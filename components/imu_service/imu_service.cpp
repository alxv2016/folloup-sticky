#include "imu_service.h"

#include <new>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"
#include "waveshare_board.h"
#include "waveshare_board_config.h"

namespace imu_service {
namespace {

constexpr const char* kTag = "ImuService";
constexpr int kDebugSampleCount = 3;
constexpr TickType_t kDebugSampleDelay = pdMS_TO_TICKS(250);
constexpr float kMilliGToG = 1.0f / 1000.0f;

i2c_master_bus_handle_t s_sensor_bus = nullptr;
Qmi8658* s_imu = nullptr;
bool s_initialized = false;

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(kTag,
             "Initializing QMI8658: addr=0x%02X bus=%d scl=GPIO%d sda=GPIO%d int=GPIO%d",
             WAVESHARE_QMI8658_I2C_ADDR, static_cast<int>(WAVESHARE_SENSOR_I2C_PORT),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SCL_PIN),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SDA_PIN),
             static_cast<int>(WAVESHARE_IMU_INT_PIN));

    esp_err_t err = waveshare_board::EnsureSensorI2cBus(&s_sensor_bus);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Sensor I2C bus unavailable: %s", esp_err_to_name(err));
        return err;
    }

    // Polling-only: the accel/gyro are read on demand (no INT2 wired), so the
    // driver runs without its interrupt task.
    s_imu = new (std::nothrow) Qmi8658(s_sensor_bus, WAVESHARE_QMI8658_I2C_ADDR);
    if (s_imu == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    err = s_imu->Initialize(true);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "QMI8658 init failed: %s", esp_err_to_name(err));
        delete s_imu;
        s_imu = nullptr;
        return err;
    }

    s_initialized = true;
    ESP_LOGI(kTag, "IMU service initialized");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

esp_err_t ReadSample(ImuSample* out_sample)
{
    if (out_sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_imu == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    float accel_mg[3] = {};
    esp_err_t err = s_imu->ReadAcceleration(accel_mg);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Accelerometer read failed: %s", esp_err_to_name(err));
        return err;
    }

    float gyro_dps[3] = {};
    err = s_imu->ReadGyroscope(gyro_dps);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Gyroscope read failed: %s", esp_err_to_name(err));
        return err;
    }

    float temperature_c = 0.0f;
    if (s_imu->ReadTemperature(&temperature_c) != ESP_OK) {
        // Temperature is best-effort; a failure here should not drop the sample.
        temperature_c = 0.0f;
    }

    ImuSample sample = {};
    sample.temperature_c = temperature_c;
    sample.accel_x_g = accel_mg[0] * kMilliGToG;
    sample.accel_y_g = accel_mg[1] * kMilliGToG;
    sample.accel_z_g = accel_mg[2] * kMilliGToG;
    sample.gyro_x_dps = gyro_dps[0];
    sample.gyro_y_dps = gyro_dps[1];
    sample.gyro_z_dps = gyro_dps[2];

    *out_sample = sample;
    return ESP_OK;
}

void LogDebugStatus()
{
    if (!s_initialized) {
        ESP_LOGW(kTag, "IMU unavailable");
        return;
    }

    for (int i = 0; i < kDebugSampleCount; ++i) {
        vTaskDelay(kDebugSampleDelay);

        ImuSample sample = {};
        const esp_err_t err = ReadSample(&sample);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Sample %d failed: %s", i + 1, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(kTag,
                 "Sample %d temp=%.2fC accel[g]={x=%.3f y=%.3f z=%.3f} gyro[dps]={x=%.3f y=%.3f z=%.3f}",
                 i + 1,
                 static_cast<double>(sample.temperature_c),
                 static_cast<double>(sample.accel_x_g),
                 static_cast<double>(sample.accel_y_g),
                 static_cast<double>(sample.accel_z_g),
                 static_cast<double>(sample.gyro_x_dps),
                 static_cast<double>(sample.gyro_y_dps),
                 static_cast<double>(sample.gyro_z_dps));
    }
}

}  // namespace imu_service
