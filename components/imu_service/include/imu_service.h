#ifndef IMU_SERVICE_H_
#define IMU_SERVICE_H_

#include "esp_err.h"

namespace imu_service {

struct ImuSample {
    float temperature_c = 0.0f;
    float accel_x_g = 0.0f;
    float accel_y_g = 0.0f;
    float accel_z_g = 0.0f;
    float gyro_x_dps = 0.0f;
    float gyro_y_dps = 0.0f;
    float gyro_z_dps = 0.0f;
    unsigned all_ones_count = 0;
    unsigned read_error_count = 0;
};

esp_err_t Init();
bool IsInitialized();
esp_err_t ReadSample(ImuSample* out_sample);
void LogDebugStatus();

}  // namespace imu_service

#endif  // IMU_SERVICE_H_
