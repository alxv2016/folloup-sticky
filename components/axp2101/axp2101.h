#ifndef __AXP2101_H__
#define __AXP2101_H__

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

#include <functional>
#include <cstdint>

#include "xpowers_axp2101_driver.h"

class Axp2101 : public Axp2101Driver {
public:
    enum class PowerKeyPressOffTime : uint8_t {
        k4S = 0,
        k6S = 1,
        k8S = 2,
        k10S = 3,
    };

    enum class PowerKeyPressOnTime : uint8_t {
        k128Ms = 0,
        k512Ms = 1,
        k1S = 2,
        k2S = 3,
    };

    enum class IrqLevelTime : uint8_t {
        k1S = 0,
        k1_5S = 1,
        k2S = 2,
        k2_5S = 3,
    };

    struct InterruptEvent {
        int level = -1;
        uint64_t irq_status = 0;
    };

    using InterruptCallback = std::function<void(const InterruptEvent&)>;

    Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr,
            gpio_num_t interrupt_pin = GPIO_NUM_NC,
            uint32_t scl_speed_hz = 100 * 1000);

    bool IsCharging();
    bool IsDischarging();
    bool IsChargingDone();
    int GetBatteryLevel();
    float GetTemperature();
    void SetPowerKeyPressOffTime(PowerKeyPressOffTime time);
    void SetPowerKeyPressOnTime(PowerKeyPressOnTime time);
    void SetIrqLevelTime(IrqLevelTime time);
    void SetButtonPowerOffEnabled(bool enabled);
    void SetButtonPowerOffRestarts(bool enabled);
    void EnablePowerKeyIrq(bool include_vbus = true);
    void ClearIrqStatus();
    uint64_t GetIrqStatus();
    void PowerOff();
    int GetInterruptLevel() const;
    void SetInterruptCallback(InterruptCallback callback);

private:
    static void IRAM_ATTR InterruptIsrHandler(void* arg);
    static void InterruptTaskEntry(void* arg);
    void InterruptTask();
    esp_err_t ConfigureInterruptPin();

    gpio_num_t interrupt_pin_ = GPIO_NUM_NC;
    QueueHandle_t interrupt_isr_queue_ = nullptr;
    TaskHandle_t interrupt_task_handle_ = nullptr;
    InterruptCallback interrupt_callback_;
};

#endif
