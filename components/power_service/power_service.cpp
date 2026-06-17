#include "power_service.h"

#include "bq27220.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "sticky_board.h"

namespace power_service {
namespace {

constexpr const char* kTag = "PowerService";
constexpr int kUsbSenseConnectedThresholdMv = 200;

i2c_master_bus_handle_t s_sensor_bus = nullptr;
i2c_master_dev_handle_t s_battery_device = nullptr;
bool s_initialized = false;
bool s_power_hold_enabled = false;
bool s_charger_enabled = false;
bool s_charge_pins_ready = false;
bool s_power_input_ready = false;
bool s_battery_ready = false;

const char* ChargeStateName(ChargeState state)
{
    switch (state) {
        case ChargeState::kCharging:
            return "charging";
        case ChargeState::kNotCharging:
            return "not_charging";
        case ChargeState::kUnknown:
        default:
            return "unknown";
    }
}

esp_err_t EnsureBatteryGauge()
{
    if (s_battery_device != nullptr && s_battery_ready) {
        return ESP_OK;
    }
    if (s_sensor_bus == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_battery_device == nullptr) {
        esp_err_t err = sticky_board::AddBq27220Device(s_sensor_bus, &s_battery_device);
        if (err != ESP_OK) {
            s_battery_device = nullptr;
            return err;
        }
    }

    if (!bq27220_probe(s_battery_device)) {
        i2c_master_bus_rm_device(s_battery_device);
        s_battery_device = nullptr;
        s_battery_ready = false;
        return ESP_ERR_NOT_FOUND;
    }

    s_battery_ready = true;
    return ESP_OK;
}

void FillBatteryStatus(BatteryStatus* battery)
{
    if (battery == nullptr) {
        return;
    }

    battery->available = false;
    if (EnsureBatteryGauge() != ESP_OK) {
        return;
    }

    Bq27220Data_t data = {};
    if (!bq27220_read_data(s_battery_device, data)) {
        ESP_LOGW(kTag, "BQ27220 telemetry read failed");
        return;
    }

    battery->available = true;
    battery->voltage_mv = data.voltage_mv;
    battery->current_ma = data.current_ma;
    battery->average_current_ma = data.average_current_ma;
    battery->remaining_capacity_mah = data.remaining_capacity_mah;
    battery->full_charge_capacity_mah = data.full_charge_capacity_mah;
    battery->state_of_charge_percent = data.state_of_charge_percent;
    battery->state_of_health_percent = data.state_of_health_percent;
    battery->cycle_count = data.cycle_count;
    battery->temperature_c = data.temperature_c;
    battery->status_bits = data.battery_status;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = EnablePowerHold();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Power hold init failed: %s", esp_err_to_name(err));
        return err;
    }

    err = sticky_board::ConfigureChargerPins();
    if (err == ESP_OK) {
        s_charge_pins_ready = true;
        err = sticky_board::SetChargerEnabled(true);
        if (err == ESP_OK) {
            s_charger_enabled = true;
        } else {
            ESP_LOGW(kTag, "Failed to enable charger: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(kTag, "Charger pin init failed: %s", esp_err_to_name(err));
    }

    err = sticky_board::InitPowerInputSense();
    if (err == ESP_OK) {
        s_power_input_ready = true;
    } else {
        ESP_LOGW(kTag, "Power-input sense init failed: %s", esp_err_to_name(err));
    }

    err = sticky_board::CreateSensorI2cBus(&s_sensor_bus);
    if (err == ESP_OK) {
        err = EnsureBatteryGauge();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "BQ27220 init failed: %s", esp_err_to_name(err));
        }
    } else {
        s_sensor_bus = nullptr;
        ESP_LOGW(kTag, "Sensor I2C bus init failed: %s", esp_err_to_name(err));
    }

    s_initialized = true;
    ESP_LOGI(kTag, "Power service initialized");
    return ESP_OK;
}

esp_err_t EnablePowerHold()
{
    if (s_power_hold_enabled) {
        return ESP_OK;
    }

    esp_err_t err = sticky_board::EnablePowerHold();
    if (err == ESP_OK) {
        s_power_hold_enabled = true;
    }
    return err;
}

esp_err_t ReadStatus(Status* out_status)
{
    if (out_status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    Status status = {};
    status.initialized = s_initialized;
    status.charger_enabled = s_charger_enabled;

    if (s_charge_pins_ready) {
        bool charging = false;
        if (sticky_board::ReadChargeState(&charging) == ESP_OK) {
            status.charge_state =
                charging ? ChargeState::kCharging : ChargeState::kNotCharging;
        }
    }

    if (s_power_input_ready) {
        int millivolts = 0;
        if (sticky_board::ReadPowerInputSenseMv(&millivolts) == ESP_OK) {
            status.power_input_valid = true;
            status.power_input_sense_mv = millivolts;
            status.usb_detected = millivolts >= kUsbSenseConnectedThresholdMv;
        }
    }

    FillBatteryStatus(&status.battery);

    *out_status = status;
    return ESP_OK;
}

void LogDebugStatus()
{
    Status status = {};
    const esp_err_t err = ReadStatus(&status);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Failed to read power status: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(kTag,
             "status: initialized=%d charger_enabled=%d charge_state=%s "
             "power_input_valid=%d power_input_sense_mv=%d usb_detected=%d",
             status.initialized ? 1 : 0, status.charger_enabled ? 1 : 0,
             ChargeStateName(status.charge_state),
             status.power_input_valid ? 1 : 0, status.power_input_sense_mv,
             status.usb_detected ? 1 : 0);

    if (!status.battery.available) {
        ESP_LOGW(kTag, "battery: unavailable");
        return;
    }

    ESP_LOGI(kTag,
             "battery: soc=%u%% soh=%u%% voltage=%umV current=%dmA "
             "avg_current=%dmA remaining=%umAh full=%umAh temp=%.2fC "
             "cycles=%u status=0x%04X",
             status.battery.state_of_charge_percent,
             status.battery.state_of_health_percent, status.battery.voltage_mv,
             status.battery.current_ma, status.battery.average_current_ma,
             status.battery.remaining_capacity_mah,
             status.battery.full_charge_capacity_mah,
             static_cast<double>(status.battery.temperature_c),
             status.battery.cycle_count, status.battery.status_bits);
}

esp_err_t SetChargerEnabled(bool enabled)
{
    if (!s_charge_pins_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = sticky_board::SetChargerEnabled(enabled);
    if (err == ESP_OK) {
        s_charger_enabled = enabled;
    }
    return err;
}

esp_err_t RequestShutdown()
{
    ESP_LOGI(kTag, "Shutdown requested");
    esp_err_t err = sticky_board::ReleasePowerHold();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Power hold release failed: %s", esp_err_to_name(err));
        sticky_board::RestorePowerHold();
    } else {
        s_power_hold_enabled = false;
    }
    return err;
}

}  // namespace power_service
