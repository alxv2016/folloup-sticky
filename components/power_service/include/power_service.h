#ifndef POWER_SERVICE_H_
#define POWER_SERVICE_H_

#include <cstdint>

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
};

struct Status {
    bool initialized = false;
    bool charger_enabled = false;
    ChargeState charge_state = ChargeState::kUnknown;
    bool power_input_valid = false;
    int power_input_sense_mv = 0;
    bool usb_detected = false;
    BatteryStatus battery = {};
};

esp_err_t Init();
esp_err_t EnablePowerHold();
esp_err_t ReadStatus(Status* out_status);
void LogDebugStatus();
esp_err_t SetChargerEnabled(bool enabled);
esp_err_t RequestShutdown();

}  // namespace power_service

#endif  // POWER_SERVICE_H_
