#include "imu_service.h"

#include <new>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lsm6ds3.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace imu_service {
namespace {

constexpr const char* kTag = "ImuService";
constexpr int kDebugSampleCount = 3;
constexpr TickType_t kDebugSampleDelay = pdMS_TO_TICKS(250);

i2c_master_bus_handle_t s_sensor_bus = nullptr;
i2c_master_dev_handle_t s_imu_device = nullptr;
LSM6DS3* s_imu = nullptr;
bool s_initialized = false;

const char* StatusName(status_t status)
{
    switch (status) {
        case IMU_SUCCESS:
            return "IMU_SUCCESS";
        case IMU_HW_ERROR:
            return "IMU_HW_ERROR";
        case IMU_NOT_SUPPORTED:
            return "IMU_NOT_SUPPORTED";
        case IMU_GENERIC_ERROR:
            return "IMU_GENERIC_ERROR";
        case IMU_OUT_OF_BOUNDS:
            return "IMU_OUT_OF_BOUNDS";
        case IMU_ALL_ONES_WARNING:
            return "IMU_ALL_ONES_WARNING";
        default:
            return "IMU_UNKNOWN_STATUS";
    }
}

esp_err_t StatusToEspError(status_t status)
{
    switch (status) {
        case IMU_SUCCESS:
            return ESP_OK;
        case IMU_NOT_SUPPORTED:
            return ESP_ERR_NOT_SUPPORTED;
        case IMU_OUT_OF_BOUNDS:
            return ESP_ERR_INVALID_ARG;
        case IMU_HW_ERROR:
        case IMU_GENERIC_ERROR:
        case IMU_ALL_ONES_WARNING:
        default:
            return ESP_FAIL;
    }
}

void ApplyBringupSettings(LSM6DS3& imu)
{
    imu.settings.accelRange = 4;
    imu.settings.accelSampleRate = 104;
    imu.settings.accelBandWidth = 100;
    imu.settings.gyroRange = 245;
    imu.settings.gyroSampleRate = 104;
    imu.settings.gyroBandWidth = 100;
}

void CleanupFailedInit()
{
    delete s_imu;
    s_imu = nullptr;

    if (s_imu_device != nullptr) {
        i2c_master_bus_rm_device(s_imu_device);
        s_imu_device = nullptr;
    }
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(kTag,
             "Initializing LSM6DS3TR-C: addr=0x%02X bus=%d scl=GPIO%d sda=GPIO%d int=GPIO%d",
             STICKY_LSM6DS3_I2C_ADDR, static_cast<int>(STICKY_SENSOR_I2C_PORT),
             static_cast<int>(STICKY_SENSOR_I2C_SCL_PIN),
             static_cast<int>(STICKY_SENSOR_I2C_SDA_PIN),
             static_cast<int>(STICKY_IMU_INT_PIN));

    esp_err_t err = sticky_board::EnsureSensorI2cBus(&s_sensor_bus);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Sensor I2C bus unavailable: %s", esp_err_to_name(err));
        return err;
    }

    err = sticky_board::AddLsm6ds3Device(s_sensor_bus, &s_imu_device);
    if (err != ESP_OK) {
        s_imu_device = nullptr;
        ESP_LOGW(kTag, "Add LSM6DS3TR-C I2C device failed: %s", esp_err_to_name(err));
        return err;
    }

    s_imu = new (std::nothrow) LSM6DS3(s_imu_device);
    if (s_imu == nullptr) {
        CleanupFailedInit();
        return ESP_ERR_NO_MEM;
    }

    uint8_t who_am_i = 0;
    status_t status = s_imu->readRegister(&who_am_i, lsm6ds3::kWhoAmIRegister);
    ESP_LOGI(kTag, "WHO_AM_I read: %s value=0x%02X", StatusName(status), who_am_i);
    if (status != IMU_SUCCESS) {
        CleanupFailedInit();
        return StatusToEspError(status);
    }
    if (who_am_i != lsm6ds3::kWhoAmIValueLsm6ds3trc &&
        who_am_i != lsm6ds3::kWhoAmIValueLsm6ds3) {
        ESP_LOGW(kTag, "Unexpected WHO_AM_I value: 0x%02X", who_am_i);
        CleanupFailedInit();
        return ESP_ERR_NOT_FOUND;
    }

    ApplyBringupSettings(*s_imu);
    SensorSettings applied_settings = {};
    status = s_imu->begin(&applied_settings);
    ESP_LOGI(kTag,
             "begin: %s accel_range=%ug accel_rate=%uHz gyro_range=%udps gyro_rate=%uHz",
             StatusName(status), applied_settings.accelRange,
             applied_settings.accelSampleRate, applied_settings.gyroRange,
             applied_settings.gyroSampleRate);
    if (status != IMU_SUCCESS) {
        CleanupFailedInit();
        return StatusToEspError(status);
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

    uint8_t status_reg = 0;
    const status_t status = s_imu->readRegister(&status_reg, lsm6ds3::kStatusRegister);
    if (status != IMU_SUCCESS) {
        ESP_LOGW(kTag, "Status register read failed: %s", StatusName(status));
        return StatusToEspError(status);
    }

    ImuSample sample = {};
    sample.temperature_c = s_imu->readTempC();
    sample.accel_x_g = s_imu->readFloatAccelX();
    sample.accel_y_g = s_imu->readFloatAccelY();
    sample.accel_z_g = s_imu->readFloatAccelZ();
    sample.gyro_x_dps = s_imu->readFloatGyroX();
    sample.gyro_y_dps = s_imu->readFloatGyroY();
    sample.gyro_z_dps = s_imu->readFloatGyroZ();
    sample.all_ones_count = s_imu->allOnesCounter;
    sample.read_error_count = s_imu->nonSuccessCounter;

    *out_sample = sample;
    ESP_LOGI(kTag, "sample status=0x%02X accel_drdy=%d gyro_drdy=%d",
             status_reg,
             (status_reg & lsm6ds3::kStatusAccelDataReadyMask) != 0 ? 1 : 0,
             (status_reg & lsm6ds3::kStatusGyroDataReadyMask) != 0 ? 1 : 0);
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
                 "Sample %d temp=%.2fC accel[g]={x=%.3f y=%.3f z=%.3f} gyro[dps]={x=%.3f y=%.3f z=%.3f} errors={all_ones=%u read=%u}",
                 i + 1,
                 static_cast<double>(sample.temperature_c),
                 static_cast<double>(sample.accel_x_g),
                 static_cast<double>(sample.accel_y_g),
                 static_cast<double>(sample.accel_z_g),
                 static_cast<double>(sample.gyro_x_dps),
                 static_cast<double>(sample.gyro_y_dps),
                 static_cast<double>(sample.gyro_z_dps),
                 sample.all_ones_count,
                 sample.read_error_count);
    }
}

}  // namespace imu_service
