#ifndef STICKY_BOARD_CONFIG_H_
#define STICKY_BOARD_CONFIG_H_

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

// Waveshare has four momentary buttons (active-low to GND): BOOT (GPIO0, left as
// the flash/boot strap), UP (GPIO4), FN (GPIO5, the OK/action button), and DOWN
// (GPIO6). The app's three logical buttons map onto UP / FN / DOWN. FN doubles as
// the light-sleep wake button (alongside the AXP2101 power-key IRQ).
#define STICKY_POWER_BUTTON_PIN       GPIO_NUM_5
#define STICKY_BUTTON_UP_PIN          GPIO_NUM_4
#define STICKY_BUTTON_DOWN_PIN        GPIO_NUM_6

// Waveshare SD card: ESP32-S3 SDMMC controller, 4-bit, on the GPIO matrix.
#define STICKY_SD_CLK_PIN             GPIO_NUM_16
#define STICKY_SD_CMD_PIN            GPIO_NUM_17
#define STICKY_SD_D0_PIN             GPIO_NUM_15
#define STICKY_SD_D1_PIN             GPIO_NUM_7
#define STICKY_SD_D2_PIN             GPIO_NUM_8
#define STICKY_SD_D3_PIN             GPIO_NUM_18

// Waveshare EPD is an SSD1677 on a dedicated SPI3 bus (write-only, no MISO). It
// is powered by the AXP2101 rails, so there is no GPIO power-enable.
#define STICKY_EPD_SPI_HOST           SPI3_HOST
#define STICKY_EPD_BUSY_PIN           GPIO_NUM_3
#define STICKY_EPD_RST_PIN            GPIO_NUM_46
#define STICKY_EPD_DC_PIN             GPIO_NUM_9
#define STICKY_EPD_CS_PIN             GPIO_NUM_10
#define STICKY_EPD_MOSI_PIN           GPIO_NUM_12
#define STICKY_EPD_MISO_PIN           GPIO_NUM_NC
#define STICKY_EPD_SCK_PIN            GPIO_NUM_11

#define STICKY_EPD_WIDTH              800
#define STICKY_EPD_HEIGHT             480
#define STICKY_EPD_BUFFER_LEN         ((STICKY_EPD_WIDTH * STICKY_EPD_HEIGHT) / 8)

#define STICKY_TOUCH_I2C_PORT         I2C_NUM_0
#define STICKY_TOUCH_I2C_SCL_PIN      GPIO_NUM_2
#define STICKY_TOUCH_I2C_SDA_PIN      GPIO_NUM_3
#define STICKY_TOUCH_POWER_EN_PIN     GPIO_NUM_42
#define STICKY_TOUCH_INT_PIN          GPIO_NUM_21
#define STICKY_TOUCH_RST_PIN          GPIO_NUM_41
#define STICKY_TOUCH_POWER_DELAY_MS   250
#define STICKY_TOUCH_LOGICAL_WIDTH    STICKY_EPD_HEIGHT
#define STICKY_TOUCH_LOGICAL_HEIGHT   STICKY_EPD_WIDTH

// Waveshare shares one I2C bus (GPIO41 SDA / GPIO42 SCL) across the AXP2101 PMIC,
// QMI8658 IMU, PCF85063 RTC, and SHTC3 (unused). Neither pin is a strapping pin.
#define STICKY_SENSOR_I2C_PORT       I2C_NUM_1
#define STICKY_SENSOR_I2C_SCL_PIN    GPIO_NUM_42
#define STICKY_SENSOR_I2C_SDA_PIN    GPIO_NUM_41

#define STICKY_AXP2101_I2C_ADDR      0x34
#define STICKY_PMIC_IRQ_PIN          GPIO_NUM_38

#define STICKY_PCF85063_I2C_ADDR      0x51

#define STICKY_QMI8658_I2C_ADDR      0x6B
#define STICKY_IMU_INT_PIN           GPIO_NUM_40

// ES8311 audio codec: I2C control shares the sensor bus above; audio streams over
// I2S0. Run full-duplex at 16 kHz to match the recording/Gemini pipeline (no
// resampling). NS4150B power-amp enable on GPIO39.
#define STICKY_AUDIO_SAMPLE_RATE_HZ  16000
#define STICKY_AUDIO_I2S_MCLK        GPIO_NUM_13
#define STICKY_AUDIO_I2S_BCLK        GPIO_NUM_14
#define STICKY_AUDIO_I2S_WS          GPIO_NUM_47
#define STICKY_AUDIO_I2S_DIN         GPIO_NUM_21
#define STICKY_AUDIO_I2S_DOUT        GPIO_NUM_48
#define STICKY_AUDIO_PA_PIN          GPIO_NUM_39

#define STICKY_I2C_GLITCH_IGNORE_CNT 7
#define STICKY_I2C_SPEED_HZ          400000

#endif  // STICKY_BOARD_CONFIG_H_
