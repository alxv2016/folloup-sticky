#include "i2c_device.h"

#include <cstring>
#include <vector>

#include <esp_log.h>

#define TAG "I2cDevice"


I2cDevice::I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr, uint32_t scl_speed_hz) {
    i2c_device_config_t i2c_device_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = scl_speed_hz,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &i2c_device_cfg, &i2c_device_));
    assert(i2c_device_ != NULL);
}

esp_err_t I2cDevice::WriteBytes(const uint8_t* data, size_t length, int timeout_ms) {
    return i2c_master_transmit(i2c_device_, data, length, timeout_ms);
}

esp_err_t I2cDevice::ReadBytes(uint8_t* buffer, size_t length, int timeout_ms) {
    return i2c_master_receive(i2c_device_, buffer, length, timeout_ms);
}

esp_err_t I2cDevice::WriteRegs(uint8_t reg, const uint8_t* data, size_t length, int timeout_ms) {
    std::vector<uint8_t> buffer(length + 1);
    buffer[0] = reg;
    if (data != nullptr && length > 0) {
        memcpy(buffer.data() + 1, data, length);
    }
    return i2c_master_transmit(i2c_device_, buffer.data(), buffer.size(), timeout_ms);
}

esp_err_t I2cDevice::ReadRegs(uint8_t reg, uint8_t* buffer, size_t length, int timeout_ms) {
    return i2c_master_transmit_receive(i2c_device_, &reg, 1, buffer, length, timeout_ms);
}

esp_err_t I2cDevice::WriteReg(uint8_t reg, uint8_t value, int timeout_ms) {
    return WriteRegs(reg, &value, 1, timeout_ms);
}

esp_err_t I2cDevice::ReadReg(uint8_t reg, uint8_t* value, int timeout_ms) {
    if (value == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return ReadRegs(reg, value, 1, timeout_ms);
}

void I2cDevice::WriteRegOrDie(uint8_t reg, uint8_t value) {
    ESP_ERROR_CHECK(WriteReg(reg, value));
}

uint8_t I2cDevice::ReadRegOrDie(uint8_t reg) {
    uint8_t value = 0;
    ESP_ERROR_CHECK(ReadReg(reg, &value));
    return value;
}
