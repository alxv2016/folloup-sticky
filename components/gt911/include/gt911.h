#ifndef GT911_H_
#define GT911_H_

#include <cstdint>
#include <functional>

#include "driver/i2c_master.h"

constexpr uint8_t GT911_ADDR_5D = 0x5D;
constexpr uint8_t GT911_ADDR_14 = 0x14;

constexpr uint16_t GT911_REG_COMMAND = 0x8040;
constexpr uint16_t GT911_REG_CONFIG_START = 0x8047;
constexpr uint16_t GT911_REG_ID = 0x8140;
constexpr uint16_t GT911_REG_COORD_RES = 0x8146;
constexpr uint16_t GT911_REG_STATUS = 0x814E;
constexpr uint16_t GT911_REG_POINTS = 0x8150;

struct GTPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t size = 0;
    uint8_t id = 0;
};

using TouchCallback = std::function<void(int8_t count, const GTPoint* points)>;

class GT911 {
public:
    static constexpr uint8_t kMaxTouchPoints = 5;

    GT911() = default;
    ~GT911();

    bool begin(int int_pin, int rst_pin, uint16_t width, uint16_t height,
               i2c_master_bus_handle_t i2c_bus);

    void onTouch(TouchCallback callback);
    void loop();

    bool is_available();
    int8_t read_points(GTPoint* points, uint8_t max_points = kMaxTouchPoints);

    void setRotation(uint8_t rotation);
    void readResolution(uint16_t& max_x, uint16_t& max_y);

    bool isReady() const { return ready_; }
    uint8_t address() const { return addr_; }

private:
    bool attachDevice(uint8_t address);
    void detachDevice();
    bool resetForAddress(uint8_t address);
    bool reset();
    bool clearStatus();

    bool writeRegister(uint16_t reg, uint8_t value);
    bool readRegister(uint16_t reg, uint8_t& value);
    bool readRegisters(uint16_t reg, uint8_t* buffer, uint8_t len);

    static uint16_t mapCoord(uint16_t value, uint16_t in_max, uint16_t out_max);

    int int_pin_ = -1;
    int rst_pin_ = -1;

    uint16_t width_ = 0;
    uint16_t height_ = 0;

    uint16_t max_x_sensor_ = 2048;
    uint16_t max_y_sensor_ = 2048;

    uint8_t addr_ = GT911_ADDR_5D;
    uint8_t rotation_ = 0;

    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    i2c_master_dev_handle_t i2c_dev_ = nullptr;
    TouchCallback callback_ = nullptr;

    bool ready_ = false;
    bool status_cached_ = false;
    uint8_t cached_status_ = 0;
};

#endif  // GT911_H_
