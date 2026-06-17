#ifndef STICKY_BOARD_H_
#define STICKY_BOARD_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace sticky_board {

esp_err_t EnablePowerHold();
esp_err_t ReleasePowerHold();
esp_err_t RestorePowerHold();

esp_err_t ConfigureChargerPins();
esp_err_t SetChargerEnabled(bool enabled);
esp_err_t ReadChargeState(bool* charging);

esp_err_t InitPowerInputSense();
esp_err_t ReadPowerInputSenseMv(int* millivolts);

esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t AddBq27220Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device);

}  // namespace sticky_board

#endif  // STICKY_BOARD_H_
