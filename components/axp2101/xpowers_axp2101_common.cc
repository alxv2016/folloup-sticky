#include "xpowers_axp2101_common.h"

#include <esp_log.h>
#include <esp_idf_version.h>

#include <cstdlib>
#include <cstring>

namespace {

constexpr const char* kTag = "Axp2101Common";

}

bool Axp2101Common::begin(i2c_master_bus_handle_t i2c_dev_bus_handle, uint8_t addr,
                          uint32_t scl_speed_hz) {
    if (has_init_) return true;
    if (i2c_dev_bus_handle == nullptr) return false;

    bus_handle_ = i2c_dev_bus_handle;
    addr_ = addr;

    i2c_device_config_t i2c_dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = scl_speed_hz,
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,3,0))
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
#endif
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle_, &i2c_dev_conf, &i2c_device_);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to add I2C device 0x%02X at %lu Hz: %s", addr_,
                 static_cast<unsigned long>(scl_speed_hz), esp_err_to_name(err));
        return false;
    }

    has_init_ = true;
    return true;
}

bool Axp2101Common::begin() {
    return i2c_device_ != nullptr;
}

void Axp2101Common::end() {
    if (i2c_device_ != nullptr) {
        i2c_master_bus_rm_device(i2c_device_);
        i2c_device_ = nullptr;
    }
    has_init_ = false;
}

int Axp2101Common::readRegister(uint8_t reg) {
    uint8_t val = 0;
    return readRegister(reg, &val, 1) == -1 ? -1 : val;
}

int Axp2101Common::writeRegister(uint8_t reg, uint8_t val) {
    return writeRegister(reg, &val, 1);
}

int Axp2101Common::readRegister(uint8_t reg, uint8_t* buf, uint8_t length) {
    if (i2c_device_ == nullptr) return -1;
    esp_err_t err = i2c_master_transmit_receive(i2c_device_, &reg, 1, buf, length, -1);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C read failed addr=0x%02X reg=0x%02X len=%u: %s", addr_, reg,
                 static_cast<unsigned>(length), esp_err_to_name(err));
        return -1;
    }
    return 0;
}

int Axp2101Common::writeRegister(uint8_t reg, uint8_t* buf, uint8_t length) {
    if (i2c_device_ == nullptr) return -1;
    uint8_t* write_buffer = static_cast<uint8_t*>(malloc(length + 1));
    if (write_buffer == nullptr) return -1;
    write_buffer[0] = reg;
    memcpy(write_buffer + 1, buf, length);
    esp_err_t err = i2c_master_transmit(i2c_device_, write_buffer, length + 1, -1);
    free(write_buffer);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "I2C write failed addr=0x%02X reg=0x%02X len=%u: %s", addr_, reg,
                 static_cast<unsigned>(length), esp_err_to_name(err));
        return -1;
    }
    return 0;
}

bool Axp2101Common::clrRegisterBit(uint8_t reg, uint8_t bit) {
    int val = readRegister(reg);
    if (val == -1) return false;
    return writeRegister(reg, static_cast<uint8_t>(val & (~(1ULL << bit)))) == 0;
}

bool Axp2101Common::setRegisterBit(uint8_t reg, uint8_t bit) {
    int val = readRegister(reg);
    if (val == -1) return false;
    return writeRegister(reg, static_cast<uint8_t>(val | (1ULL << bit))) == 0;
}

bool Axp2101Common::getRegisterBit(uint8_t reg, uint8_t bit) {
    int val = readRegister(reg);
    if (val == -1) return false;
    return (val & (1ULL << bit)) != 0;
}

uint16_t Axp2101Common::readRegisterH8L4(uint8_t high_reg, uint8_t low_reg) {
    int h8 = readRegister(high_reg);
    int l4 = readRegister(low_reg);
    if (h8 == -1 || l4 == -1) return 0;
    return (h8 << 4) | (l4 & 0x0F);
}

uint16_t Axp2101Common::readRegisterH8L5(uint8_t high_reg, uint8_t low_reg) {
    int h8 = readRegister(high_reg);
    int l5 = readRegister(low_reg);
    if (h8 == -1 || l5 == -1) return 0;
    return (h8 << 5) | (l5 & 0x1F);
}

uint16_t Axp2101Common::readRegisterH6L8(uint8_t high_reg, uint8_t low_reg) {
    int h6 = readRegister(high_reg);
    int l8 = readRegister(low_reg);
    if (h6 == -1 || l8 == -1) return 0;
    return ((h6 & 0x3F) << 8) | l8;
}

uint16_t Axp2101Common::readRegisterH5L8(uint8_t high_reg, uint8_t low_reg) {
    int h5 = readRegister(high_reg);
    int l8 = readRegister(low_reg);
    if (h5 == -1 || l8 == -1) return 0;
    return ((h5 & 0x1F) << 8) | l8;
}
