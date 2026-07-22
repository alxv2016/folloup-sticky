#ifndef AXP2101_COMMON_H
#define AXP2101_COMMON_H

#include <driver/i2c_master.h>

#include <cstdint>

class Axp2101Common {
protected:
    bool begin(i2c_master_bus_handle_t i2c_dev_bus_handle, uint8_t addr,
               uint32_t scl_speed_hz);
    bool begin();
    void end();

    int readRegister(uint8_t reg);
    int writeRegister(uint8_t reg, uint8_t val);
    int readRegister(uint8_t reg, uint8_t* buf, uint8_t length);
    int writeRegister(uint8_t reg, uint8_t* buf, uint8_t length);

    bool clrRegisterBit(uint8_t reg, uint8_t bit);
    bool setRegisterBit(uint8_t reg, uint8_t bit);
    bool getRegisterBit(uint8_t reg, uint8_t bit);

    uint16_t readRegisterH8L4(uint8_t high_reg, uint8_t low_reg);
    uint16_t readRegisterH8L5(uint8_t high_reg, uint8_t low_reg);
    uint16_t readRegisterH6L8(uint8_t high_reg, uint8_t low_reg);
    uint16_t readRegisterH5L8(uint8_t high_reg, uint8_t low_reg);

protected:
    bool has_init_ = false;
    i2c_master_bus_handle_t bus_handle_ = nullptr;
    i2c_master_dev_handle_t i2c_device_ = nullptr;
    uint8_t addr_ = 0xFF;
};

#endif  // AXP2101_COMMON_H
