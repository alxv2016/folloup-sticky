#include "gt911.h"

#include <algorithm>
#include <array>
#include <utility>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* kTag = "GT911";
constexpr uint32_t kI2cFreqHz = 100000;
constexpr int kI2cTimeoutMs = 20;
constexpr uint32_t kPostResetSettleMs = 30;
constexpr uint32_t kResetHoldLowMs = 20;
constexpr uint32_t kResetReleaseHighMs = 20;
constexpr uint32_t kIntSyncLowMs = 50;
constexpr uint32_t kResetAddressSettleMs = 80;
constexpr uint32_t kResetRetryDelayMs = 20;
constexpr int kResetRetryCount = 3;

uint64_t MonotonicMs()
{
    return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace

GT911::~GT911()
{
    detachDevice();
}

bool GT911::begin(int int_pin, int rst_pin, uint16_t width, uint16_t height,
                  i2c_master_bus_handle_t i2c_bus)
{
    if (int_pin < 0 || rst_pin < 0 || i2c_bus == nullptr) {
        ESP_LOGW(kTag, "begin rejected: int=%d rst=%d bus=%p map=%ux%u",
                 int_pin, rst_pin, static_cast<void*>(i2c_bus),
                 static_cast<unsigned>(width), static_cast<unsigned>(height));
        return false;
    }

    const uint64_t start_ms = MonotonicMs();
    ESP_LOGI(kTag, "begin: int=%d rst=%d map=%ux%u bus=%p", int_pin, rst_pin,
             static_cast<unsigned>(width), static_cast<unsigned>(height),
             static_cast<void*>(i2c_bus));

    int_pin_ = int_pin;
    rst_pin_ = rst_pin;
    width_ = width;
    height_ = height;
    i2c_bus_ = i2c_bus;

    rotation_ = 0;
    status_cached_ = false;
    cached_status_ = 0;
    ready_ = false;

    gpio_set_direction(static_cast<gpio_num_t>(int_pin_), GPIO_MODE_OUTPUT);
    gpio_set_direction(static_cast<gpio_num_t>(rst_pin_), GPIO_MODE_OUTPUT);

    bool reset_ok = false;
    for (int attempt = 0; attempt < kResetRetryCount; ++attempt) {
        ESP_LOGI(kTag, "reset attempt %d/%d", attempt + 1, kResetRetryCount);
        if (reset()) {
            reset_ok = true;
            break;
        }

        ESP_LOGW(kTag, "reset attempt %d/%d failed", attempt + 1,
                 kResetRetryCount);
        vTaskDelay(pdMS_TO_TICKS(kResetRetryDelayMs));
    }

    if (!reset_ok) {
        ESP_LOGW(kTag, "begin failed: GT911 not found after %d attempt(s)",
                 kResetRetryCount);
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(kPostResetSettleMs));

    uint16_t max_x = 0;
    uint16_t max_y = 0;
    readResolution(max_x, max_y);
    if (!clearStatus()) {
        ESP_LOGW(kTag, "begin: clear status failed after reset");
    }

    ready_ = true;
    ESP_LOGI(kTag, "begin ok: addr=0x%02X sensor=%ux%u map=%ux%u elapsed=%llums",
             addr_, static_cast<unsigned>(max_x_sensor_),
             static_cast<unsigned>(max_y_sensor_), static_cast<unsigned>(width_),
             static_cast<unsigned>(height_),
             static_cast<unsigned long long>(MonotonicMs() - start_ms));
    return true;
}

void GT911::onTouch(TouchCallback callback)
{
    callback_ = std::move(callback);
}

void GT911::setRotation(uint8_t rotation)
{
    rotation_ = static_cast<uint8_t>(rotation & 0x03U);
}

void GT911::readResolution(uint16_t& max_x, uint16_t& max_y)
{
    uint8_t buffer[4] = {};
    if (readRegisters(GT911_REG_COORD_RES, buffer, sizeof(buffer))) {
        const uint16_t x =
            static_cast<uint16_t>((static_cast<uint16_t>(buffer[1]) << 8) | buffer[0]);
        const uint16_t y =
            static_cast<uint16_t>((static_cast<uint16_t>(buffer[3]) << 8) | buffer[2]);
        if (x != 0) {
            max_x_sensor_ = x;
        }
        if (y != 0) {
            max_y_sensor_ = y;
        }
        ESP_LOGI(kTag, "resolution register: raw=%02X %02X %02X %02X sensor=%ux%u",
                 buffer[0], buffer[1], buffer[2], buffer[3],
                 static_cast<unsigned>(max_x_sensor_),
                 static_cast<unsigned>(max_y_sensor_));
    } else {
        ESP_LOGW(kTag, "resolution read failed, keeping sensor=%ux%u",
                 static_cast<unsigned>(max_x_sensor_),
                 static_cast<unsigned>(max_y_sensor_));
    }

    max_x = max_x_sensor_;
    max_y = max_y_sensor_;
}

bool GT911::is_available()
{
    if (i2c_dev_ == nullptr) {
        ESP_LOGW(kTag, "is_available: i2c device is null");
        return false;
    }

    if (status_cached_) {
        const uint8_t cached_count = cached_status_ & 0x0FU;
        return ((cached_status_ & 0x80U) != 0U) && cached_count > 0U &&
               cached_count <= kMaxTouchPoints;
    }

    uint8_t status = 0;
    if (!readRegister(GT911_REG_STATUS, status)) {
        ESP_LOGW(kTag, "is_available: status read failed addr=0x%02X", addr_);
        return false;
    }

    ESP_LOGI(kTag, "status read: addr=0x%02X status=0x%02X ready=%u count=%u",
             addr_, status, static_cast<unsigned>((status & 0x80U) != 0U),
             static_cast<unsigned>(status & 0x0FU));

    if ((status & 0x80U) == 0U) {
        return false;
    }

    const uint8_t touch_count = status & 0x0FU;
    if (touch_count == 0U) {
        ESP_LOGI(kTag, "status ready with no active touch, clearing status");
        (void)clearStatus();
        return false;
    }
    if (touch_count > kMaxTouchPoints) {
        ESP_LOGW(kTag, "status invalid touch count: status=0x%02X count=%u",
                 status, static_cast<unsigned>(touch_count));
        (void)clearStatus();
        return false;
    }

    cached_status_ = status;
    status_cached_ = true;
    return true;
}

int8_t GT911::read_points(GTPoint* points, uint8_t max_points)
{
    if (i2c_dev_ == nullptr) {
        ESP_LOGW(kTag, "read_points: i2c device is null");
        return -1;
    }

    uint8_t status = 0;
    if (status_cached_) {
        status = cached_status_;
        status_cached_ = false;
        cached_status_ = 0;
    } else if (!readRegister(GT911_REG_STATUS, status)) {
        ESP_LOGW(kTag, "read_points: status read failed addr=0x%02X", addr_);
        return -1;
    }

    ESP_LOGI(kTag, "read_points: addr=0x%02X status=0x%02X ready=%u count=%u max=%u",
             addr_, status, static_cast<unsigned>((status & 0x80U) != 0U),
             static_cast<unsigned>(status & 0x0FU),
             static_cast<unsigned>(max_points));

    if ((status & 0x80U) == 0U) {
        return 0;
    }

    const uint8_t touch_count = status & 0x0FU;
    if (touch_count == 0U) {
        ESP_LOGI(kTag, "read_points: ready with no active touch, clearing status");
        (void)clearStatus();
        return 0;
    }
    if (touch_count > kMaxTouchPoints) {
        ESP_LOGW(kTag, "read_points: invalid touch count status=0x%02X count=%u",
                 status, static_cast<unsigned>(touch_count));
        (void)clearStatus();
        return 0;
    }

    std::array<uint8_t, kMaxTouchPoints * 8> raw_data = {};
    const uint8_t read_length = static_cast<uint8_t>(touch_count * 8U);
    if (!readRegisters(GT911_REG_POINTS, raw_data.data(), read_length)) {
        ESP_LOGW(kTag, "read_points: point data read failed addr=0x%02X len=%u",
                 addr_, static_cast<unsigned>(read_length));
        (void)clearStatus();
        return -1;
    }

    const uint8_t out_count = std::min(touch_count, max_points);
    if (points != nullptr) {
        for (uint8_t i = 0; i < out_count; ++i) {
            const uint8_t* point_data = &raw_data[static_cast<size_t>(i) * 8U];

            const uint16_t x = static_cast<uint16_t>(
                point_data[0] | (static_cast<uint16_t>(point_data[1]) << 8));
            const uint16_t y = static_cast<uint16_t>(
                point_data[2] | (static_cast<uint16_t>(point_data[3]) << 8));
            const uint16_t size = static_cast<uint16_t>(
                point_data[4] | (static_cast<uint16_t>(point_data[5]) << 8));
            const uint8_t id = point_data[7];

            const uint16_t map_x = mapCoord(x, max_x_sensor_, width_);
            const uint16_t map_y = mapCoord(y, max_y_sensor_, height_);

            uint16_t final_x = map_x;
            uint16_t final_y = map_y;
            switch (rotation_) {
                case 1:
                    final_x = map_y;
                    final_y = (map_x > width_) ? 0U : static_cast<uint16_t>(width_ - map_x);
                    break;
                case 2:
                    final_x = (map_x > width_) ? 0U : static_cast<uint16_t>(width_ - map_x);
                    final_y = (map_y > height_) ? 0U : static_cast<uint16_t>(height_ - map_y);
                    break;
                case 3:
                    final_x = (map_y > height_) ? 0U : static_cast<uint16_t>(height_ - map_y);
                    final_y = map_x;
                    break;
                default:
                    break;
            }

            points[i].x = final_x;
            points[i].y = final_y;
            points[i].id = id;
            points[i].size = size;
        }
    }

    if (!clearStatus()) {
        ESP_LOGW(kTag, "read_points: clear status failed after %u point(s)",
                 static_cast<unsigned>(out_count));
    }
    return static_cast<int8_t>(out_count);
}

void GT911::loop()
{
    if (callback_ == nullptr || !is_available()) {
        return;
    }

    GTPoint points[kMaxTouchPoints] = {};
    const int8_t touch_count = read_points(points, kMaxTouchPoints);
    if (touch_count > 0) {
        callback_(touch_count, points);
    }
}

bool GT911::reset()
{
    if (i2c_bus_ == nullptr) {
        ESP_LOGW(kTag, "reset: i2c bus is null");
        return false;
    }

    constexpr uint8_t kProbeAddrs[] = {GT911_ADDR_14, GT911_ADDR_5D};
    uint8_t id[4] = {};
    for (uint8_t candidate : kProbeAddrs) {
        ESP_LOGI(kTag, "reset: try candidate addr=0x%02X", candidate);
        if (!resetForAddress(candidate)) {
            ESP_LOGW(kTag, "reset: resetForAddress failed addr=0x%02X", candidate);
            continue;
        }

        const esp_err_t probe_err = i2c_master_probe(i2c_bus_, candidate, kI2cTimeoutMs);
        if (probe_err != ESP_OK) {
            ESP_LOGW(kTag, "reset: probe failed addr=0x%02X err=%s", candidate,
                     esp_err_to_name(probe_err));
            continue;
        }

        if (!attachDevice(candidate)) {
            ESP_LOGW(kTag, "reset: attach device failed addr=0x%02X", candidate);
            continue;
        }

        if (readRegisters(GT911_REG_ID, id, sizeof(id))) {
            ESP_LOGI(kTag, "reset: id read ok addr=0x%02X id='%c%c%c%c' raw=%02X %02X %02X %02X",
                     candidate, id[0], id[1], id[2], id[3],
                     id[0], id[1], id[2], id[3]);
            return true;
        }

        ESP_LOGW(kTag, "reset: id read failed addr=0x%02X", candidate);
        detachDevice();
    }

    return false;
}

bool GT911::resetForAddress(uint8_t address)
{
    const int int_level = (address == GT911_ADDR_5D) ? 0 : 1;
    ESP_LOGI(kTag, "resetForAddress: addr=0x%02X int_pin=%d rst_pin=%d int_level=%d",
             address, int_pin_, rst_pin_, int_level);

    gpio_hold_dis(static_cast<gpio_num_t>(rst_pin_));
    gpio_hold_dis(static_cast<gpio_num_t>(int_pin_));

    gpio_set_direction(static_cast<gpio_num_t>(rst_pin_), GPIO_MODE_OUTPUT);
    gpio_set_direction(static_cast<gpio_num_t>(int_pin_), GPIO_MODE_OUTPUT);

    gpio_set_level(static_cast<gpio_num_t>(rst_pin_), 0);
    gpio_set_level(static_cast<gpio_num_t>(int_pin_), int_level);
    vTaskDelay(pdMS_TO_TICKS(kResetHoldLowMs));

    gpio_set_level(static_cast<gpio_num_t>(rst_pin_), 1);
    vTaskDelay(pdMS_TO_TICKS(kResetReleaseHighMs));

    // Keep this Goodix INT sync pulse. Without it, GT911 can answer product ID
    // reads but fail to load/report resolution or touch data; see
    // docs/gt911-touch-reset-debugging.md.
    gpio_set_level(static_cast<gpio_num_t>(int_pin_), 0);
    vTaskDelay(pdMS_TO_TICKS(kIntSyncLowMs));

    gpio_set_direction(static_cast<gpio_num_t>(int_pin_), GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(kResetAddressSettleMs));

    return true;
}

bool GT911::clearStatus()
{
    status_cached_ = false;
    cached_status_ = 0;
    const bool ok = writeRegister(GT911_REG_STATUS, 0);
    if (ok) {
        ESP_LOGI(kTag, "clear status ok addr=0x%02X", addr_);
    }
    return ok;
}

bool GT911::writeRegister(uint16_t reg, uint8_t value)
{
    if (i2c_dev_ == nullptr) {
        ESP_LOGW(kTag, "writeRegister: i2c device is null reg=0x%04X val=0x%02X",
                 reg, value);
        return false;
    }

    const uint8_t payload[] = {
        static_cast<uint8_t>(reg >> 8),
        static_cast<uint8_t>(reg & 0xFFU),
        value,
    };
    const esp_err_t err = i2c_master_transmit(i2c_dev_, payload, sizeof(payload),
                                              kI2cTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "writeRegister failed addr=0x%02X reg=0x%04X val=0x%02X err=%s",
                 addr_, reg, value, esp_err_to_name(err));
        return false;
    }
    return true;
}

bool GT911::readRegister(uint16_t reg, uint8_t& value)
{
    return readRegisters(reg, &value, 1);
}

bool GT911::readRegisters(uint16_t reg, uint8_t* buffer, uint8_t len)
{
    if (i2c_dev_ == nullptr || buffer == nullptr || len == 0) {
        ESP_LOGW(kTag, "readRegisters rejected: dev=%p buffer=%p reg=0x%04X len=%u",
                 static_cast<void*>(i2c_dev_), static_cast<void*>(buffer),
                 reg, static_cast<unsigned>(len));
        return false;
    }

    const uint8_t reg_buf[] = {
        static_cast<uint8_t>(reg >> 8),
        static_cast<uint8_t>(reg & 0xFFU),
    };
    const esp_err_t err = i2c_master_transmit_receive(i2c_dev_, reg_buf,
                                                      sizeof(reg_buf), buffer, len,
                                                      kI2cTimeoutMs);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "readRegisters failed addr=0x%02X reg=0x%04X len=%u err=%s",
                 addr_, reg, static_cast<unsigned>(len), esp_err_to_name(err));
        return false;
    }
    return true;
}

bool GT911::attachDevice(uint8_t address)
{
    if (i2c_bus_ == nullptr) {
        ESP_LOGW(kTag, "attachDevice: i2c bus is null addr=0x%02X", address);
        return false;
    }

    detachDevice();

    i2c_device_config_t device_config = {};
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = address;
    device_config.scl_speed_hz = kI2cFreqHz;

    const esp_err_t err =
        i2c_master_bus_add_device(i2c_bus_, &device_config, &i2c_dev_);
    if (err != ESP_OK) {
        i2c_dev_ = nullptr;
        ESP_LOGW(kTag, "attachDevice failed addr=0x%02X err=%s", address,
                 esp_err_to_name(err));
        return false;
    }

    addr_ = address;
    ESP_LOGI(kTag, "attachDevice ok addr=0x%02X freq=%u", addr_,
             static_cast<unsigned>(kI2cFreqHz));
    return true;
}

void GT911::detachDevice()
{
    if (i2c_dev_ == nullptr) {
        return;
    }

    i2c_master_bus_rm_device(i2c_dev_);
    i2c_dev_ = nullptr;
}

uint16_t GT911::mapCoord(uint16_t value, uint16_t in_max, uint16_t out_max)
{
    if (in_max == 0) {
        return 0;
    }

    const uint32_t mapped = (static_cast<uint32_t>(value) *
                             static_cast<uint32_t>(out_max)) /
                            static_cast<uint32_t>(in_max);
    return static_cast<uint16_t>(mapped > 0xFFFFU ? 0xFFFFU : mapped);
}
