#ifndef PCF8563_H_
#define PCF8563_H_

#include <cstddef>
#include <cstdint>
#include <ctime>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace pcf8563 {

struct InterruptStatus {
    uint8_t raw = 0;
    bool alarm_flag = false;
    bool timer_flag = false;
    bool alarm_interrupt_enabled = false;
    bool timer_interrupt_enabled = false;
};

bool Probe(i2c_master_dev_handle_t i2c_dev);
bool DisableClkout(i2c_master_dev_handle_t i2c_dev);
bool GetTime(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo);
bool SetTime(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo);
esp_err_t ReadInterruptStatus(i2c_master_dev_handle_t i2c_dev,
                              InterruptStatus* out_status);
esp_err_t ClearInterrupts(i2c_master_dev_handle_t i2c_dev);
esp_err_t DisableAlarmAndTimerInterrupts(i2c_master_dev_handle_t i2c_dev);
esp_err_t ClearAndDisableInterrupts(i2c_master_dev_handle_t i2c_dev);

}  // namespace pcf8563

bool pcf8563_probe(i2c_master_dev_handle_t i2c_dev);
bool pcf8563_disable_clkout(i2c_master_dev_handle_t i2c_dev);
bool pcf8563_get_time(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo);
bool pcf8563_set_time(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo);

#endif  // PCF8563_H_
