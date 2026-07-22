#include "power_service.h"

#include <mutex>

#include "axp2101.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcf85063.h"
#include "sticky_board.h"

namespace power_service {
namespace {

constexpr const char* kTag = "PowerService";
constexpr uint16_t kLowBatteryPercentThreshold = 10;
constexpr TickType_t kShutdownSettleDelay = pdMS_TO_TICKS(200);

// Battery/RTC telemetry rides the shared sensor I2C bus and reads slowly under
// load. A dedicated low-priority task polls it in the background and caches the
// last-good values, so UI refreshes (status bar) never do synchronous sensor I2C
// and never block on bus contention during SD writes / display refreshes.
constexpr TickType_t kSensorPollInterval = pdMS_TO_TICKS(2000);
constexpr uint32_t kSensorPollTaskStackWords = 3072;
constexpr uint32_t kSensorFailWarnThreshold = 5;

i2c_master_bus_handle_t s_sensor_bus = nullptr;
i2c_master_dev_handle_t s_rtc_device = nullptr;
bool s_initialized = false;
bool s_power_hold_enabled = false;
bool s_charger_enabled = false;
bool s_rtc_ready = false;

// AXP2101-derived power state, cached alongside the battery/RTC snapshot so
// ReadStatus (on the UI refresh path) never touches I2C.
struct PowerSnapshot {
    ChargeState charge_state = ChargeState::kUnknown;
    bool vbus_valid = false;
    bool usb_detected = false;
    int vbus_mv = 0;
};

// Cached telemetry published by the poll task and copied (never read via I2C) by
// ReadStatus. The mutex is held only for the struct copies, never across I2C.
std::mutex s_snapshot_mutex;
BatteryStatus s_cached_battery = {};
RtcStatus s_cached_rtc = {};
PowerSnapshot s_cached_power = {};
TaskHandle_t s_sensor_task = nullptr;
uint32_t s_battery_fail_streak = 0;
uint32_t s_rtc_fail_streak = 0;

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

const char* WakeupCauseName(esp_sleep_wakeup_cause_t cause)
{
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT1:
            return "ext1";
        case ESP_SLEEP_WAKEUP_GPIO:
            return "gpio";
        case ESP_SLEEP_WAKEUP_TIMER:
            return "timer";
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            return "undefined";
        default:
            return "other";
    }
}

esp_err_t EnsureRtc()
{
    if (s_rtc_device != nullptr && s_rtc_ready) {
        return ESP_OK;
    }
    if (s_sensor_bus == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rtc_device == nullptr) {
        esp_err_t err = sticky_board::AddPcf85063Device(s_sensor_bus, &s_rtc_device);
        if (err != ESP_OK) {
            s_rtc_device = nullptr;
            return err;
        }
    }

    if (!pcf85063::Probe(s_rtc_device)) {
        i2c_master_bus_rm_device(s_rtc_device);
        s_rtc_device = nullptr;
        s_rtc_ready = false;
        return ESP_ERR_NOT_FOUND;
    }

    if (!pcf85063::DisableClkout(s_rtc_device)) {
        ESP_LOGW(kTag, "PCF85063 CLKOUT disable failed");
    }

    s_rtc_ready = true;
    return ESP_OK;
}

void FillRtcStatus(RtcStatus* rtc)
{
    if (rtc == nullptr) {
        return;
    }

    rtc->available = false;
    if (EnsureRtc() != ESP_OK) {
        return;
    }

    pcf85063::InterruptStatus interrupt_status = {};
    esp_err_t err = pcf85063::ReadInterruptStatus(s_rtc_device, &interrupt_status);
    if (err != ESP_OK) {
        // Transient contention is expected; the poll task warns only if it persists.
        ESP_LOGD(kTag, "PCF85063 interrupt status read failed: %s",
                 esp_err_to_name(err));
        return;
    }

    rtc->available = true;
    rtc->control_status_2 = interrupt_status.raw;
    rtc->alarm_flag = interrupt_status.alarm_flag;
    rtc->timer_flag = interrupt_status.timer_flag;
    rtc->alarm_interrupt_enabled = interrupt_status.alarm_interrupt_enabled;
    rtc->timer_interrupt_enabled = interrupt_status.timer_interrupt_enabled;
}

void FillBatteryStatus(BatteryStatus* battery)
{
    if (battery == nullptr) {
        return;
    }

    battery->available = false;
    Axp2101* pmic = sticky_board::GetPmic();
    if (pmic == nullptr || !pmic->isBatteryConnect()) {
        return;
    }

    const int soc = pmic->GetBatteryLevel();
    if (soc < 0) {
        // Gauge not ready / no calibrated pack yet.
        return;
    }

    battery->available = true;
    battery->voltage_mv = pmic->getBattVoltage();
    battery->state_of_charge_percent = static_cast<uint16_t>(soc);
    battery->temperature_c = pmic->GetTemperature();
    battery->full_charge_detected = pmic->IsChargingDone();
    battery->low_battery_10_percent = soc <= kLowBatteryPercentThreshold;
}

void FillPowerSnapshot(PowerSnapshot* power)
{
    if (power == nullptr) {
        return;
    }

    *power = PowerSnapshot{};
    Axp2101* pmic = sticky_board::GetPmic();
    if (pmic == nullptr) {
        return;
    }

    power->charge_state =
        pmic->IsCharging() ? ChargeState::kCharging : ChargeState::kNotCharging;

    // The PMIC always reports VBUS presence, so the sample is unconditionally
    // valid — callers use usb_detected to know whether external power is present.
    power->vbus_valid = true;
    power->usb_detected = pmic->isVbusIn();
    power->vbus_mv = power->usb_detected ? pmic->getVbusVoltage() : 0;
}

// Reads battery + RTC + PMIC power state over I2C and publishes them to the
// cache. A failed battery/RTC read keeps the previous last-good value (so the UI
// never flickers to "unavailable" on a single blip); a sustained run of failures
// is logged once.
void PollSensorsOnce()
{
    BatteryStatus battery = {};
    FillBatteryStatus(&battery);

    RtcStatus rtc = {};
    FillRtcStatus(&rtc);

    PowerSnapshot power = {};
    FillPowerSnapshot(&power);

    {
        std::lock_guard<std::mutex> lock(s_snapshot_mutex);
        if (battery.available) {
            s_cached_battery = battery;
        }
        if (rtc.available) {
            s_cached_rtc = rtc;
        }
        s_cached_power = power;
    }

    if (battery.available) {
        s_battery_fail_streak = 0;
    } else if (++s_battery_fail_streak == kSensorFailWarnThreshold) {
        ESP_LOGW(kTag, "Battery telemetry unavailable for %u consecutive polls",
                 static_cast<unsigned>(s_battery_fail_streak));
    }

    if (rtc.available) {
        s_rtc_fail_streak = 0;
    } else if (++s_rtc_fail_streak == kSensorFailWarnThreshold) {
        ESP_LOGW(kTag, "PCF85063 status unavailable for %u consecutive polls",
                 static_cast<unsigned>(s_rtc_fail_streak));
    }
}

void SensorPollTask(void*)
{
    for (;;) {
        PollSensorsOnce();
        vTaskDelay(kSensorPollInterval);
    }
}

esp_err_t ClearRtcInterruptsForShutdown()
{
    esp_err_t err = EnsureRtc();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "PCF85063 unavailable before shutdown: %s",
                 esp_err_to_name(err));
        return err;
    }

    pcf85063::InterruptStatus before = {};
    err = pcf85063::ReadInterruptStatus(s_rtc_device, &before);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "PCF85063 pre-shutdown interrupt read failed: %s",
                 esp_err_to_name(err));
        return err;
    }

    err = pcf85063::ClearAndDisableInterrupts(s_rtc_device);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "PCF85063 interrupt clear failed: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    const esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(kTag, "Wakeup cause: %s (%d)", WakeupCauseName(wakeup_cause),
             static_cast<int>(wakeup_cause));

    esp_err_t err = EnablePowerHold();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Power hold init failed: %s", esp_err_to_name(err));
        return err;
    }
    // The AXP2101 rail bring-up in the board layer leaves the charger enabled.
    s_charger_enabled = true;

    err = sticky_board::EnsureSensorI2cBus(&s_sensor_bus);
    if (err == ESP_OK) {
        err = EnsureRtc();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "PCF85063 init failed: %s", esp_err_to_name(err));
        }
    } else {
        s_sensor_bus = nullptr;
        ESP_LOGW(kTag, "Sensor I2C bus init failed: %s", esp_err_to_name(err));
    }

    // Warm the telemetry cache once (the bus is quiet at boot) and start the
    // background poller so later UI refreshes never do synchronous sensor I2C.
    PollSensorsOnce();
    if (s_sensor_task == nullptr) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            SensorPollTask,
            "sensor_poll",
            kSensorPollTaskStackWords,
            nullptr,
            followup_task_config::kPrioritySensorPoll,
            &s_sensor_task,
            followup_task_config::kSystemCore);
        if (created != pdPASS) {
            s_sensor_task = nullptr;
            ESP_LOGW(kTag, "Sensor poll task create failed; telemetry will be stale");
        }
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

    // Battery/RTC/power come from the background poll cache so this call (on the
    // UI refresh path) never blocks on sensor I2C.
    {
        std::lock_guard<std::mutex> lock(s_snapshot_mutex);
        status.battery = s_cached_battery;
        status.rtc = s_cached_rtc;
        status.charge_state = s_cached_power.charge_state;
        status.power_input_valid = s_cached_power.vbus_valid;
        status.usb_detected = s_cached_power.usb_detected;
        status.power_input_sense_mv = s_cached_power.vbus_mv;
    }

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
             "power_input_valid=%d vbus_mv=%d usb_detected=%d",
             status.initialized ? 1 : 0, status.charger_enabled ? 1 : 0,
             ChargeStateName(status.charge_state),
             status.power_input_valid ? 1 : 0, status.power_input_sense_mv,
             status.usb_detected ? 1 : 0);

    if (status.rtc.available) {
        ESP_LOGI(kTag,
                 "rtc: cs2=0x%02X alarm_flag=%d timer_flag=%d alarm_ie=%d "
                 "timer_ie=%d",
                 status.rtc.control_status_2, status.rtc.alarm_flag ? 1 : 0,
                 status.rtc.timer_flag ? 1 : 0,
                 status.rtc.alarm_interrupt_enabled ? 1 : 0,
                 status.rtc.timer_interrupt_enabled ? 1 : 0);
    } else {
        ESP_LOGW(kTag, "rtc: unavailable");
    }

    if (!status.battery.available) {
        ESP_LOGW(kTag, "battery: unavailable");
        return;
    }

    ESP_LOGI(kTag,
             "battery: soc=%u%% voltage=%umV temp=%.2fC full_charge=%d "
             "low_battery_10=%d",
             status.battery.state_of_charge_percent, status.battery.voltage_mv,
             static_cast<double>(status.battery.temperature_c),
             status.battery.full_charge_detected ? 1 : 0,
             status.battery.low_battery_10_percent ? 1 : 0);
}

esp_err_t SetChargerEnabled(bool enabled)
{
    // The AXP2101 charger is configured during rail bring-up and stays enabled;
    // there is no separate charge-enable GPIO on this board. Track the requested
    // state so callers still see a consistent flag.
    s_charger_enabled = enabled;
    return ESP_OK;
}

esp_err_t ReadRtcTime(std::tm* out_time)
{
    if (out_time == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = EnsureRtc();
    if (err != ESP_OK) {
        return err;
    }

    std::tm timeinfo = {};
    if (!pcf85063::GetTime(s_rtc_device, timeinfo)) {
        return ESP_FAIL;
    }

    *out_time = timeinfo;
    return ESP_OK;
}

esp_err_t WriteRtcTime(const std::tm& time)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = EnsureRtc();
    if (err != ESP_OK) {
        return err;
    }

    return pcf85063::SetTime(s_rtc_device, time) ? ESP_OK : ESP_FAIL;
}

esp_err_t RequestShutdown()
{
    ESP_LOGI(kTag, "Shutdown requested");
    esp_err_t rtc_err = ClearRtcInterruptsForShutdown();
    if (rtc_err != ESP_OK) {
        ESP_LOGW(kTag, "Continuing shutdown after RTC clear failure");
    }

    Axp2101* pmic = sticky_board::GetPmic();
    if (pmic == nullptr) {
        ESP_LOGE(kTag, "PMIC unavailable; cannot power off");
        return ESP_ERR_INVALID_STATE;
    }

    // The AXP2101 cuts every rail on shutdown(). On battery the board goes dark
    // here and never returns; while VBUS is present the PMIC keeps the rail fed
    // and the call returns, leaving the device running (as intended — you cannot
    // fully power off a USB-powered board).
    s_power_hold_enabled = false;
    ESP_LOGW(kTag, "Powering off via AXP2101 (unplug USB to fully power down)");
    vTaskDelay(kShutdownSettleDelay);
    pmic->PowerOff();

    ESP_LOGW(kTag, "AXP2101 power-off returned; board still powered (VBUS present)");
    return ESP_OK;
}

}  // namespace power_service
