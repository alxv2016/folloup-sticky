#ifndef MICROPHONE_SERVICE_H_
#define MICROPHONE_SERVICE_H_

#include <cstddef>
#include <cstdint>

#include "esp_err.h"

namespace microphone_service {

constexpr uint32_t kSampleRateHz = 16000;

esp_err_t Init();
bool IsInitialized();
esp_err_t Enable();
esp_err_t Disable();
esp_err_t ReadSamples(int16_t* dest, size_t sample_count, size_t* samples_read,
                      uint32_t timeout_ms);
uint8_t CalculateLevelPercent(const int16_t* samples, size_t sample_count);
uint8_t LastInputLevelPercent();
void LogDebugStatus();

}  // namespace microphone_service

#endif  // MICROPHONE_SERVICE_H_
