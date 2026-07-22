#ifndef I2C_DEVICE_H
#define I2C_DEVICE_H

#include <driver/i2c_master.h>
#include <esp_err.h>

class I2cDevice {
public:
    explicit I2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr,
                       uint32_t scl_speed_hz = 400 * 1000);

protected:
    i2c_master_dev_handle_t i2c_device_;

    esp_err_t WriteBytes(const uint8_t* data, size_t length, int timeout_ms = 100);
    esp_err_t ReadBytes(uint8_t* buffer, size_t length, int timeout_ms = 100);
    esp_err_t WriteRegs(uint8_t reg, const uint8_t* data, size_t length, int timeout_ms = 100);
    esp_err_t ReadRegs(uint8_t reg, uint8_t* buffer, size_t length, int timeout_ms = 100);
    esp_err_t WriteReg(uint8_t reg, uint8_t value, int timeout_ms = 100);
    esp_err_t ReadReg(uint8_t reg, uint8_t* value, int timeout_ms = 100);

    void WriteRegOrDie(uint8_t reg, uint8_t value);
    uint8_t ReadRegOrDie(uint8_t reg);
};

#endif // I2C_DEVICE_H
