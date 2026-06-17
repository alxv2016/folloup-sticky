#ifndef STICKY_BOARD_CONFIG_H_
#define STICKY_BOARD_CONFIG_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// reTerminal Sticky power and BQ27220 fuel-gauge wiring.

#define STICKY_POWER_BUTTON_PIN       GPIO_NUM_4
#define STICKY_POWER_HOLD_PIN         GPIO_NUM_45
#define STICKY_POWER_LOCK_PIN         GPIO_NUM_46

#define STICKY_BATTERY_CHARGE_EN_PIN  GPIO_NUM_39
#define STICKY_CHARGE_STATE_PIN       GPIO_NUM_40
#define STICKY_POWER_INPUT_VOLT_PIN   GPIO_NUM_9

#define STICKY_SENSOR_I2C_PORT       I2C_NUM_1
#define STICKY_SENSOR_I2C_SCL_PIN    GPIO_NUM_0
#define STICKY_SENSOR_I2C_SDA_PIN    GPIO_NUM_1

#define STICKY_BQ27220_I2C_ADDR      0x55
#define STICKY_BQ27220_INT_PIN       GPIO_NUM_7

#define STICKY_I2C_GLITCH_IGNORE_CNT 7
#define STICKY_I2C_SPEED_HZ          400000

#endif  // STICKY_BOARD_CONFIG_H_
