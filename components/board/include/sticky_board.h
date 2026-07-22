#ifndef STICKY_BOARD_H_
#define STICKY_BOARD_H_

#include <cstdint>

#include "audio_codec.h"
#include "axp2101.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace sticky_board {

// Waveshare power is managed entirely by the AXP2101 PMIC on the shared I2C bus:
// EnablePowerHold() brings up its rails/charger, and GetPmic() exposes it for
// battery/charge/VBUS telemetry and software power-off.
esp_err_t EnablePowerHold();
Axp2101* GetPmic();

// Full-duplex ES8311 audio codec (I2C control on the shared sensor bus, audio on
// I2S0). Lazily constructed on first call and owned by the board; returns nullptr
// if the shared I2C bus is unavailable. Shared by capture (recording) and playback.
AudioCodec* GetAudioCodec();

esp_err_t EnableTouchPower();
esp_err_t ConfigureTouchInterruptPin(gpio_int_type_t intr_type);
esp_err_t ReadTouchInterruptLevel(int* level);

esp_err_t EnsureSensorI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t CreateTouchI2cBus(i2c_master_bus_handle_t* out_bus);
esp_err_t AddPcf85063Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device);

}  // namespace sticky_board

#endif  // STICKY_BOARD_H_
