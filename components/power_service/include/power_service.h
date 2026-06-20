#ifndef POWER_SERVICE_H_
#define POWER_SERVICE_H_

#include <cstdint>
#include <ctime>

#include "esp_err.h"

namespace power_service {

enum class ChargeState {
    kUnknown,
    kCharging,
    kNotCharging,
};

struct BatteryStatus {
    bool available = false;
    uint16_t voltage_mv = 0;
    int16_t current_ma = 0;
    int16_t average_current_ma = 0;
    uint16_t remaining_capacity_mah = 0;
    uint16_t full_charge_capacity_mah = 0;
    uint16_t state_of_charge_percent = 0;
    uint16_t state_of_health_percent = 0;
    uint16_t cycle_count = 0;
    float temperature_c = 0.0f;
    uint16_t status_bits = 0;
    bool full_charge_detected = false;
    bool low_battery_10_percent = false;
    bool operation_status_available = false;
    uint16_t operation_status_bits = 0;
    uint8_t operation_security_state = 0;
    bool operation_config_update = false;
    bool operation_btp_interrupt = false;
    bool btp_thresholds_available = false;
    uint16_t btp_discharge_set = 0;
    uint16_t btp_charge_set = 0;
    bool interrupt_level_available = false;
    int interrupt_level = 0;
};

struct RtcStatus {
    bool available = false;
    uint8_t control_status_2 = 0;
    bool alarm_flag = false;
    bool timer_flag = false;
    bool alarm_interrupt_enabled = false;
    bool timer_interrupt_enabled = false;
};

struct Status {
    bool initialized = false;
    bool charger_enabled = false;
    ChargeState charge_state = ChargeState::kUnknown;
    bool power_input_valid = false;
    int power_input_sense_mv = 0;
    int power_input_raw_average = 0;
    int power_input_raw_min = 0;
    int power_input_raw_max = 0;
    int power_input_sample_count = 0;
    bool usb_detected = false;
    RtcStatus rtc = {};
    BatteryStatus battery = {};
};

esp_err_t Init();
esp_err_t EnablePowerHold();
esp_err_t ReadStatus(Status* out_status);
void LogDebugStatus();
esp_err_t SetChargerEnabled(bool enabled);
esp_err_t ReadRtcTime(std::tm* out_time);
esp_err_t WriteRtcTime(const std::tm& time);
esp_err_t RequestShutdown();

}  // namespace power_service

#endif  // POWER_SERVICE_H_
