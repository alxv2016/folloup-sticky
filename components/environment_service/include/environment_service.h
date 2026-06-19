#ifndef ENVIRONMENT_SERVICE_H_
#define ENVIRONMENT_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace environment_service {

struct EnvironmentSample {
    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
    uint8_t address = 0;
    uint32_t serial_number = 0;
};

esp_err_t Init();
bool IsInitialized();
esp_err_t ReadSample(EnvironmentSample* out_sample);
void LogDebugStatus();

}  // namespace environment_service

#endif  // ENVIRONMENT_SERVICE_H_
