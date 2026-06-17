#include "pcf8563.h"

namespace {

constexpr int kI2cTimeoutMs = 20;
constexpr uint8_t kControlStatus1Reg = 0x00;
constexpr uint8_t kControlStatus2Reg = 0x01;
constexpr uint8_t kTimeReg = 0x02;
constexpr uint8_t kAlarmMinuteReg = 0x09;
constexpr uint8_t kClkoutControlReg = 0x0D;
constexpr uint8_t kTimerControlReg = 0x0E;
constexpr uint8_t kTimerReg = 0x0F;
constexpr uint8_t kClkoutDisabled = 0x00;
constexpr uint8_t kAlarmDisabled = 0x80;
constexpr uint8_t kControlStatus2TimerInterruptEnable = 1U << 0;
constexpr uint8_t kControlStatus2AlarmInterruptEnable = 1U << 1;
constexpr uint8_t kControlStatus2TimerFlag = 1U << 2;
constexpr uint8_t kControlStatus2AlarmFlag = 1U << 3;

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
           full_year >= 1900 && full_year <= 2099;
}

}  // namespace

namespace pcf8563 {

bool Probe(i2c_master_dev_handle_t i2c_dev)
{
    uint8_t dummy = 0;
    return ReadReg(i2c_dev, kControlStatus1Reg, &dummy, 1) == ESP_OK;
}

bool DisableClkout(i2c_master_dev_handle_t i2c_dev)
{
    return WriteReg(i2c_dev, kClkoutControlReg, kClkoutDisabled) == ESP_OK;
}

bool GetTime(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo)
{
    uint8_t raw[7] = {};
    if (ReadReg(i2c_dev, kTimeReg, raw, sizeof(raw)) != ESP_OK) {
        return false;
    }

    timeinfo = {};
    timeinfo.tm_sec = BcdToDec(raw[0] & 0x7FU);
    timeinfo.tm_min = BcdToDec(raw[1] & 0x7FU);
    timeinfo.tm_hour = BcdToDec(raw[2] & 0x3FU);
    timeinfo.tm_mday = BcdToDec(raw[3] & 0x3FU);
    timeinfo.tm_wday = BcdToDec(raw[4] & 0x07U);
    timeinfo.tm_mon = static_cast<int>(BcdToDec(raw[5] & 0x1FU)) - 1;

    const int year = BcdToDec(raw[6]);
    timeinfo.tm_year = ((raw[5] & 0x80U) != 0U) ? year : (year + 100);
    timeinfo.tm_isdst = -1;

    return true;
}

bool SetTime(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo)
{
    if (!ValidTime(timeinfo)) {
        return false;
    }

    const int full_year = timeinfo.tm_year + 1900;
    const uint8_t century_bit = full_year < 2000 ? 0x80U : 0x00U;
    const uint8_t payload[] = {
        kTimeReg,
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_sec)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_min)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_hour)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_mday)),
        DecToBcd(static_cast<uint8_t>(timeinfo.tm_wday)),
        static_cast<uint8_t>(DecToBcd(static_cast<uint8_t>(timeinfo.tm_mon + 1)) |
                             century_bit),
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
    esp_err_t err = ReadReg(i2c_dev, kControlStatus2Reg, &raw, 1);
    if (err != ESP_OK) {
        return err;
    }

    out_status->raw = raw;
    out_status->alarm_flag = (raw & kControlStatus2AlarmFlag) != 0;
    out_status->timer_flag = (raw & kControlStatus2TimerFlag) != 0;
    out_status->alarm_interrupt_enabled =
        (raw & kControlStatus2AlarmInterruptEnable) != 0;
    out_status->timer_interrupt_enabled =
        (raw & kControlStatus2TimerInterruptEnable) != 0;
    return ESP_OK;
}

esp_err_t ClearInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    InterruptStatus status = {};
    esp_err_t err = ReadInterruptStatus(i2c_dev, &status);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t cleared =
        status.raw & ~(kControlStatus2AlarmFlag | kControlStatus2TimerFlag);
    return WriteReg(i2c_dev, kControlStatus2Reg, cleared);
}

esp_err_t DisableAlarmAndTimerInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    esp_err_t err = WriteReg(i2c_dev, kControlStatus2Reg, 0x00);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t alarm_payload[] = {
        kAlarmMinuteReg,
        kAlarmDisabled,
        kAlarmDisabled,
        kAlarmDisabled,
        kAlarmDisabled,
    };
    err = WriteBytes(i2c_dev, alarm_payload, sizeof(alarm_payload));
    if (err != ESP_OK) {
        return err;
    }

    err = WriteReg(i2c_dev, kTimerControlReg, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    return WriteReg(i2c_dev, kTimerReg, 0x00);
}

esp_err_t ClearAndDisableInterrupts(i2c_master_dev_handle_t i2c_dev)
{
    esp_err_t err = ClearInterrupts(i2c_dev);
    if (err != ESP_OK) {
        return err;
    }
    return DisableAlarmAndTimerInterrupts(i2c_dev);
}

}  // namespace pcf8563

bool pcf8563_probe(i2c_master_dev_handle_t i2c_dev)
{
    return pcf8563::Probe(i2c_dev);
}

bool pcf8563_disable_clkout(i2c_master_dev_handle_t i2c_dev)
{
    return pcf8563::DisableClkout(i2c_dev);
}

bool pcf8563_get_time(i2c_master_dev_handle_t i2c_dev, std::tm& timeinfo)
{
    return pcf8563::GetTime(i2c_dev, timeinfo);
}

bool pcf8563_set_time(i2c_master_dev_handle_t i2c_dev, const std::tm& timeinfo)
{
    return pcf8563::SetTime(i2c_dev, timeinfo);
}
