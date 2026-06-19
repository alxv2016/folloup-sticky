#include "sht40.h"

#include <algorithm>
#include <cstddef>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* kTag = "SHT40";
constexpr int kI2cTimeoutMs = 100;
constexpr TickType_t kSerialNumberDelay = pdMS_TO_TICKS(10);
constexpr uint8_t kSerialNumberCommand = 0x89;
constexpr uint8_t kSoftResetCommand = 0x94;
constexpr uint8_t kMeasureHighPrecisionCommand = 0xFD;
constexpr uint8_t kMeasureMediumPrecisionCommand = 0xF6;
constexpr uint8_t kMeasureLowPrecisionCommand = 0xE0;

esp_err_t write_command(i2c_master_dev_handle_t i2c_dev, uint8_t command)
{
    const esp_err_t err = i2c_master_transmit(i2c_dev, &command, sizeof(command), kI2cTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "command 0x%02X write failed: %s", command, esp_err_to_name(err));
    }
    return err;
}

esp_err_t read_bytes(i2c_master_dev_handle_t i2c_dev, uint8_t* buffer, size_t length)
{
    const esp_err_t err = i2c_master_receive(i2c_dev, buffer, length, kI2cTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "read %u bytes failed: %s",
                 static_cast<unsigned>(length), esp_err_to_name(err));
    }
    return err;
}

bool check_crc(const uint8_t* data, size_t length, uint8_t checksum)
{
    if (data == nullptr || length == 0) {
        return false;
    }

    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80U) != 0U) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x31U);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc == checksum;
}

uint8_t measure_command_for_precision(sht40::Precision precision)
{
    switch (precision) {
        case sht40::Precision::kMedium:
            return kMeasureMediumPrecisionCommand;
        case sht40::Precision::kLow:
            return kMeasureLowPrecisionCommand;
        case sht40::Precision::kHigh:
        default:
            return kMeasureHighPrecisionCommand;
    }
}

TickType_t measure_delay_for_precision(sht40::Precision precision)
{
    switch (precision) {
        case sht40::Precision::kMedium:
            return pdMS_TO_TICKS(5);
        case sht40::Precision::kLow:
            return pdMS_TO_TICKS(2);
        case sht40::Precision::kHigh:
        default:
            return pdMS_TO_TICKS(10);
    }
}

float raw_to_temperature_c(uint16_t raw_temperature)
{
    return -45.0f + 175.0f * static_cast<float>(raw_temperature) / 65535.0f;
}

float raw_to_humidity_percent(uint16_t raw_humidity)
{
    const float humidity = -6.0f + 125.0f * static_cast<float>(raw_humidity) / 65535.0f;
    return std::clamp(humidity, 0.0f, 100.0f);
}

}  // namespace

namespace sht40 {

esp_err_t probe(i2c_master_dev_handle_t i2c_dev)
{
    uint32_t serial_number = 0;
    return read_serial_number(i2c_dev, serial_number);
}

esp_err_t soft_reset(i2c_master_dev_handle_t i2c_dev)
{
    esp_err_t err = write_command(i2c_dev, kSoftResetCommand);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
    return ESP_OK;
}

esp_err_t read_serial_number(i2c_master_dev_handle_t i2c_dev, uint32_t& serial_number)
{
    serial_number = 0;

    esp_err_t err = write_command(i2c_dev, kSerialNumberCommand);
    if (err != ESP_OK) {
        return err;
    }

    vTaskDelay(kSerialNumberDelay);

    uint8_t response[6] = {};
    err = read_bytes(i2c_dev, response, sizeof(response));
    if (err != ESP_OK) {
        return err;
    }

    const bool serial_msb_crc_ok = check_crc(response, 2, response[2]);
    const bool serial_lsb_crc_ok = check_crc(response + 3, 2, response[5]);
    if (!serial_msb_crc_ok || !serial_lsb_crc_ok) {
        ESP_LOGW(kTag,
                 "serial CRC failed: raw=%02X %02X %02X %02X %02X %02X crc_ok={%d,%d}",
                 response[0], response[1], response[2], response[3], response[4], response[5],
                 serial_msb_crc_ok ? 1 : 0, serial_lsb_crc_ok ? 1 : 0);
        return ESP_ERR_INVALID_CRC;
    }

    serial_number = (static_cast<uint32_t>(response[0]) << 24) |
                    (static_cast<uint32_t>(response[1]) << 16) |
                    (static_cast<uint32_t>(response[3]) << 8) |
                    static_cast<uint32_t>(response[4]);
    return ESP_OK;
}

esp_err_t read_measurement(i2c_master_dev_handle_t i2c_dev, Measurement& measurement,
                           Precision precision, uint8_t retries)
{
    const uint8_t command = measure_command_for_precision(precision);
    esp_err_t last_err = ESP_FAIL;

    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        esp_err_t err = write_command(i2c_dev, command);
        if (err != ESP_OK) {
            last_err = err;
            continue;
        }

        vTaskDelay(measure_delay_for_precision(precision));

        uint8_t response[6] = {};
        err = read_bytes(i2c_dev, response, sizeof(response));
        if (err != ESP_OK) {
            last_err = err;
            continue;
        }

        const bool temperature_crc_ok = check_crc(response, 2, response[2]);
        const bool humidity_crc_ok = check_crc(response + 3, 2, response[5]);
        if (!temperature_crc_ok || !humidity_crc_ok) {
            ESP_LOGW(kTag,
                     "measurement CRC failed: raw=%02X %02X %02X %02X %02X %02X crc_ok={%d,%d}",
                     response[0], response[1], response[2],
                     response[3], response[4], response[5],
                     temperature_crc_ok ? 1 : 0, humidity_crc_ok ? 1 : 0);
            last_err = ESP_ERR_INVALID_CRC;
            continue;
        }

        const uint16_t raw_temperature = static_cast<uint16_t>((response[0] << 8) | response[1]);
        const uint16_t raw_humidity = static_cast<uint16_t>((response[3] << 8) | response[4]);

        measurement.temperature_c = raw_to_temperature_c(raw_temperature);
        measurement.humidity_percent = raw_to_humidity_percent(raw_humidity);
        return ESP_OK;
    }

    return last_err;
}

}  // namespace sht40
