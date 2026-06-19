#ifndef STICKY_BOARD_H_
#define STICKY_BOARD_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace sticky_board {

struct PowerInputSample {
    int raw_average = 0;
    int raw_min = 0;
    int raw_max = 0;
    int calibrated_mv = 0;
    int sample_count = 0;
};

esp_err_t EnablePowerHold();
esp_err_t ReleasePowerHold();
esp_err_t RestorePowerHold();

esp_err_t ConfigureChargerPins();
esp_err_t SetChargerEnabled(bool enabled);
esp_err_t ReadChargeState(bool* charging);

esp_err_t InitPowerInputSense();
esp_err_t ReadPowerInputSample(PowerInputSample* out_sample);

esp_err_t ConfigureBq27220InterruptPin();
esp_err_t ReadBq27220InterruptLevel(int* level);

esp_err_t EnsureSharedSpiBus();
esp_err_t EnableEpaperPower();
esp_err_t EnableTouchPower();
esp_err_t ConfigureTouchInterruptPin(gpio_int_type_t intr_type);
esp_err_t ReadTouchInterruptLevel(int* level);

esp_err_t EnsureSensorI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t CreateTouchI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t AddBq27220Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device);
esp_err_t AddPcf8563Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device);
esp_err_t AddLsm6ds3Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device);

}  // namespace sticky_board

#endif  // STICKY_BOARD_H_
