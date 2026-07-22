#include "pcf85063.h"

namespace {

constexpr int kI2cTimeoutMs = 20;

constexpr uint8_t kControl1Reg = 0x00;
constexpr uint8_t kControl2Reg = 0x01;
constexpr uint8_t kSecondsReg = 0x04;  // time block: sec,min,hour,day,wday,mon,year
constexpr uint8_t kSecondAlarmReg = 0x0B;
constexpr uint8_t kTimerModeReg = 0x11;

// Control_2 bit map.
constexpr uint8_t kControl2AlarmInterruptEnable = 0x80;  // AIE
constexpr uint8_t kControl2AlarmFlag = 0x40;             // AF
constexpr uint8_t kControl2TimerFlag = 0x08;             // TF
constexpr uint8_t kControl2ClkoutDisable = 0x07;         // COF=111 -> CLKOUT off

// Alarm registers disable when the per-register AEN_x (bit7) is set.
constexpr uint8_t kAlarmDisabled = 0x80;

uint8_t BcdToDec(uint8_t value)
{
    return static_cast<uint8_t>(((value >> 4) * 10U) + (value & 0x0FU));
}

uint8_t DecToBcd(uint8_t value)
{
    return static_cast<uint8_t>(((value / 10U) << 4) | (value % 10U));
}

esp_err_t WriteBytes(i2c_master_dev_handle_t i2c_dev, const uint8_t* data,
                     size_t len)
{
    if (i2c_dev == nullptr || data == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit(i2c_dev, data, len, kI2cTimeoutMs);
}

esp_err_t ReadReg(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint8_t* data,
                  size_t len)
{
    if (i2c_dev == nullptr || data == nullptr || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_master_transmit_receive(i2c_dev, &reg, sizeof(reg), data, len,
                                       kI2cTimeoutMs);
}

esp_err_t WriteReg(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint8_t value)
{
    const uint8_t payload[] = {reg, value};
    return WriteBytes(i2c_dev, payload, sizeof(payload));
}

bool ValidTime(const std::tm& timeinfo)
{
    const int full_year = timeinfo.tm_year + 1900;
    return timeinfo.tm_sec >= 0 && timeinfo.tm_sec <= 59 &&
           timeinfo.tm_min >= 0 && timeinfo.tm_min <= 59 &&
           timeinfo.tm_hour >= 0 && timeinfo.tm_hour <= 23 &&
           timeinfo.tm_mday >= 1 && timeinfo.tm_mday <= 31 &&
           timeinfo.tm_mon >= 0 && timeinfo.tm_mon <= 11 &&
           timeinfo.tm_wday >= 0 && timeinfo.tm_wday <= 6 &&
           full_year >= 2000 && full_year <= 2099;
}

}  // namespace

namespace pcf85063 {

bool Probe(i2c_master_dev_handle_t i2c_dev)
{
    uint8_t dummy = 0;
    return ReadReg(i2c_dev, kControl1Reg, &dummy, 1) == ESP_OK;
}

bool DisableClkout(i2c_master_dev_handle_t i2c_dev)
{
    // PCF85063's CLKOUT is controlled by COF[2:0] in Control_2 (unlike the PCF8563
    // which used a dedicated register); COF=111 disables the output.
    uint8_t control2 = 0;
    if (ReadReg(i2c_dev, kControl2Reg, &control2, 1) != ESP_OK) {
        return false;
    }
    return WriteReg(i2c_dev, kControl2Reg, control2 | kControl2ClkoutDisable) == ESP_OK;
}

bool GetTime(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo)
{
    uint8_t raw[7] = {};
    if (ReadReg(i2c_dev, kSecondsReg, raw, sizeof(raw)) != ESP_OK) {
        return false;
    }

    timeinfo = {};
    timeinfo.tm_sec = BcdToDec(raw[0] & 0x7FU);   // bit7 is the OS (osc-stop) flag
    timeinfo.tm_min = BcdToDec(raw[1] & 0x7FU);
    timeinfo.tm_hour = BcdToDec(raw[2] & 0x3FU);
    timeinfo.tm_mday = BcdToDec(raw[3] & 0x3FU);
    timeinfo.tm_wday = BcdToDec(raw[4] & 0x07U);
    timeinfo.tm_mon = static_cast<int>(BcdToDec(raw[5] & 0x1FU)) - 1;
    // The PCF85063 has no century bit; the pack is a 2000-2099 device.
    timeinfo.tm_year = static_cast<int>(BcdToDec(raw[6])) + 100;
    timeinfo.tm_isdst = -1;

    return true;
}

bool SetTime(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo)
{
    if (!ValidTime(timeinfo)) {
        return false;
    }

    const int full_year = timeinfo.tm_year + 1900;
    const uint8_t payload[] = {
        kSecondsReg,
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_sec)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_min)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_hour)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_mday)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_wday)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_mon + 1)),
        DecToBcd(static_cast<uint8_t>(full_year % 100)),
    };

    return WriteBytes(i2c_dev, payload, sizeof(payload)) == ESP_OK;
}

esp_err_t ReadInterruptStatus(i2c_master_dev_handle_t i2c_dev,
                              InterruptStatus* out_status)
{
    if (out_status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw = 0;
    esp_err_t err = ReadReg(i2c_dev, kControl2Reg, &raw, 1);
    if (err != ESP_OK) {
        return err;
    }

    out_status->raw = raw;
    out_status->alarm_flag = (raw & kControl2AlarmFlag) != 0;
    out_status->timer_flag = (raw & kControl2TimerFlag) != 0;
    out_status->alarm_interrupt_enabled =
        (raw & kControl2AlarmInterruptEnable) != 0;

    // The timer interrupt-enable (TIE) lives in the Timer_mode register.
    uint8_t timer_mode = 0;
    out_status->timer_interrupt_enabled =
        ReadReg(i2c_dev, kTimerModeReg, &timer_mode, 1) == ESP_OK &&
        (timer_mode & 0x02U) != 0;
    return ESP_OK;
}

esp_err_t ClearInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    uint8_t control2 = 0;
    esp_err_t err = ReadReg(i2c_dev, kControl2Reg, &control2, 1);
    if (err != ESP_OK) {
        return err;
    }

    // AF/TF are cleared by writing 0; preserve AIE and the COF (CLKOUT) bits.
    const uint8_t cleared =
        control2 & static_cast<uint8_t>(~(kControl2AlarmFlag | kControl2TimerFlag));
    return WriteReg(i2c_dev, kControl2Reg, cleared);
}

esp_err_t DisableAlarmAndTimerInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    // Clear AIE and both flags while keeping the CLKOUT (COF) configuration.
    uint8_t control2 = 0;
    esp_err_t err = ReadReg(i2c_dev, kControl2Reg, &control2, 1);
    if (err != ESP_OK) {
        return err;
    }
    control2 &= static_cast<uint8_t>(
        ~(kControl2AlarmInterruptEnable | kControl2AlarmFlag | kControl2TimerFlag));
    err = WriteReg(i2c_dev, kControl2Reg, control2);
    if (err != ESP_OK) {
        return err;
    }

    // Disable every alarm register (AEN_x = 1) — second, minute, hour, day, weekday.
    const uint8_t alarm_payload[] = {
        kSecondAlarmReg,
        kAlarmDisabled,
        kAlarmDisabled,
        kAlarmDisabled,
        kAlarmDisabled,
        kAlarmDisabled,
    };
    err = WriteBytes(i2c_dev, alarm_payload, sizeof(alarm_payload));
    if (err != ESP_OK) {
        return err;
    }

    // Stop the countdown timer (TE=0, TIE=0).
    return WriteReg(i2c_dev, kTimerModeReg, 0x00);
}

esp_err_t ClearAndDisableInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    esp_err_t err = ClearInterrupts(i2c_dev);
    if (err != ESP_OK) {
        return err;
    }
    return DisableAlarmAndTimerInterrupts(i2c_dev);
}

}  // namespace pcf85063
