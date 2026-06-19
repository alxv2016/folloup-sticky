#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace sht40 {

constexpr uint8_t kPrimaryI2cAddress = 0x44;
constexpr uint8_t kAlternateI2cAddress = 0x45;

enum class Precision : uint8_t {
    kHigh,
    kMedium,
    kLow,
};

struct Measurement {
    float temperature_c = 0.0f;
    float humidity_percent = 0.0f;
};

esp_err_t probe(i2c_master_dev_handle_t i2c_dev);
esp_err_t soft_reset(i2c_master_dev_handle_t i2c_dev);
esp_err_t read_serial_number(i2c_master_dev_handle_t i2c_dev, uint32_t& serial_number);
esp_err_t read_measurement(i2c_master_dev_handle_t i2c_dev, Measurement& measurement,
                           Precision precision = Precision::kHigh, uint8_t retries = 3);

}  // namespace sht40
