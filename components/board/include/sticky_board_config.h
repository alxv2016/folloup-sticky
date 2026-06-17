#ifndef STICKY_BOARD_CONFIG_H_
#define STICKY_BOARD_CONFIG_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"

// reTerminal Sticky BQ27220 fuel-gauge wiring.

#define STICKY_SENSOR_I2C_PORT       I2C_NUM_1
#define STICKY_SENSOR_I2C_SCL_PIN    GPIO_NUM_0
#define STICKY_SENSOR_I2C_SDA_PIN    GPIO_NUM_1

#define STICKY_BQ27220_I2C_ADDR      0x55
#define STICKY_BQ27220_INT_PIN       GPIO_NUM_7

#define STICKY_I2C_GLITCH_IGNORE_CNT 7
#define STICKY_I2C_SPEED_HZ          400000

#endif  // STICKY_BOARD_CONFIG_H_
