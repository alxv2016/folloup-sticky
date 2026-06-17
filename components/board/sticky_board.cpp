#include "sticky_board.h"

#include "sticky_board_config.h"

namespace sticky_board {
namespace {

esp_err_t CreateI2cBus(i2c_port_num_t port, gpio_num_t scl_pin, gpio_num_t sda_pin,
                       i2c_master_bus_handle_t* out_bus)
{
    if (out_bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_master_bus_config_t config = {};
    config.i2c_port = port;
    config.scl_io_num = scl_pin;
    config.sda_io_num = sda_pin;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = STICKY_I2C_GLITCH_IGNORE_CNT;
    config.flags.enable_internal_pullup = 1;

    return i2c_new_master_bus(&config, out_bus);
}

}  // namespace

esp_err_t CreateSensorI2cBus(i2c_master_bus_handle_t* out_bus)
{
    // GPIO0 is also an ESP32-S3 strapping pin, so call this after startup
    // levels are no longer part of the boot-mode decision.
    return CreateI2cBus(STICKY_SENSOR_I2C_PORT, STICKY_SENSOR_I2C_SCL_PIN,
                        STICKY_SENSOR_I2C_SDA_PIN, out_bus);
}

esp_err_t AddBq27220Device(i2c_master_bus_handle_t bus,
                           i2c_master_dev_handle_t* out_device)
{
    if (bus == nullptr || out_device == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = STICKY_BQ27220_I2C_ADDR;
    config.scl_speed_hz = STICKY_I2C_SPEED_HZ;

    return i2c_master_bus_add_device(bus, &config, out_device);
}

}  // namespace sticky_board
