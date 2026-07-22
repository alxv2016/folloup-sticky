#ifndef PCF85063_H_
#define PCF85063_H_

#include <cstddef>
#include <cstdint>
#include <ctime>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Free-function driver for the NXP PCF85063 RTC on the Waveshare board. It mirrors
// the interface the power service expects (previously the PCF8563 on Sticky) so the
// service layer is register-map agnostic; only the chip underneath changed.
namespace pcf85063 {

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

}  // namespace pcf85063

#endif  // PCF85063_H_
