#include "axp2101.h"

#include <cassert>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <driver/gpio.h>
#include <esp_check.h>
#include <esp_log.h>

namespace {

constexpr const char* kTag = "Axp2101";
constexpr size_t kInterruptQueueDepth = 8;
constexpr UBaseType_t kInterruptTaskPriority = 2;
constexpr BaseType_t kInterruptTaskCore = 1;

void LogInterruptStatus(const Axp2101::InterruptEvent& event) {
    const uint64_t status = event.irq_status;
    if (status == 0) {
        return;
    }

    ESP_LOGI(kTag, "IRQ event level=%d status=0x%06llX", event.level,
             static_cast<unsigned long long>(status));

    if ((status & XPOWERS_AXP2101_PKEY_SHORT_IRQ) != 0) {
        ESP_LOGI(kTag, "Power key short press");
    }
    if ((status & XPOWERS_AXP2101_PKEY_LONG_IRQ) != 0) {
        ESP_LOGI(kTag, "Power key long press");
    }
    if ((status & XPOWERS_AXP2101_PKEY_POSITIVE_IRQ) != 0) {
        ESP_LOGI(kTag, "Power key press down");
    }
    if ((status & XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ) != 0) {
        ESP_LOGI(kTag, "Power key release");
    }
    if ((status & XPOWERS_AXP2101_VBUS_INSERT_IRQ) != 0) {
        ESP_LOGI(kTag, "VBUS inserted");
    }
    if ((status & XPOWERS_AXP2101_VBUS_REMOVE_IRQ) != 0) {
        ESP_LOGI(kTag, "VBUS removed");
    }
}

}  // namespace

Axp2101::Axp2101(i2c_master_bus_handle_t i2c_bus, uint8_t addr,
                 gpio_num_t interrupt_pin, uint32_t scl_speed_hz)
    : Axp2101Driver(i2c_bus, addr, scl_speed_hz), interrupt_pin_(interrupt_pin) {
    if (interrupt_pin_ == GPIO_NUM_NC) {
        return;
    }

    interrupt_isr_queue_ = xQueueCreate(kInterruptQueueDepth, sizeof(uint32_t));
    ESP_ERROR_CHECK(ConfigureInterruptPin());
    BaseType_t created =
        xTaskCreatePinnedToCore(InterruptTaskEntry, "axp2101_irq", 3072, this,
                                kInterruptTaskPriority, &interrupt_task_handle_,
                                kInterruptTaskCore);
    assert(created == pdPASS);
}

bool Axp2101::IsCharging() {
    return isCharging();
}

bool Axp2101::IsDischarging() {
    return isDischarge();
}

bool Axp2101::IsChargingDone() {
    return getChargerStatus() == XPOWERS_AXP2101_CHG_DONE_STATE;
}

int Axp2101::GetBatteryLevel() {
    return getBatteryPercent();
}

float Axp2101::GetTemperature() {
    return getTemperature();
}

void Axp2101::SetPowerKeyPressOffTime(PowerKeyPressOffTime time) {
    Axp2101Driver::setPowerKeyPressOffTime(static_cast<xpowers_press_off_time_t>(time));
}

void Axp2101::SetPowerKeyPressOnTime(PowerKeyPressOnTime time) {
    Axp2101Driver::setPowerKeyPressOnTime(static_cast<xpowers_press_on_time_t>(time));
}

void Axp2101::SetIrqLevelTime(IrqLevelTime time) {
    Axp2101Driver::setIrqLevelTime(static_cast<xpowers_irq_time_t>(time));
}

void Axp2101::SetButtonPowerOffEnabled(bool enabled) {
    if (enabled) {
        enablePwronShutPMIC();
    } else {
        disablePwronShutPMIC();
    }
}

void Axp2101::SetButtonPowerOffRestarts(bool enabled) {
    if (enabled) {
        enablePwrOkPinPullLow();
    } else {
        disablePwrOkPinPullLow();
    }
}

void Axp2101::EnablePowerKeyIrq(bool include_vbus) {
    disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    uint64_t irq_mask = XPOWERS_AXP2101_PKEY_SHORT_IRQ | XPOWERS_AXP2101_PKEY_LONG_IRQ |
                        XPOWERS_AXP2101_PKEY_POSITIVE_IRQ |
                        XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ;
    if (include_vbus) {
        irq_mask |= XPOWERS_AXP2101_VBUS_INSERT_IRQ | XPOWERS_AXP2101_VBUS_REMOVE_IRQ;
    }
    enableIRQ(irq_mask);
}

void Axp2101::ClearIrqStatus() {
    clearIrqStatus();
}

uint64_t Axp2101::GetIrqStatus() {
    return getIrqStatus();
}

void Axp2101::PowerOff() {
    shutdown();
}

int Axp2101::GetInterruptLevel() const {
    if (interrupt_pin_ == GPIO_NUM_NC) {
        return -1;
    }
    return gpio_get_level(interrupt_pin_);
}

void Axp2101::SetInterruptCallback(InterruptCallback callback) {
    interrupt_callback_ = std::move(callback);
}

void IRAM_ATTR Axp2101::InterruptIsrHandler(void* arg) {
    auto* pmic = static_cast<Axp2101*>(arg);
    if (pmic == nullptr || pmic->interrupt_isr_queue_ == nullptr) {
        return;
    }

    uint32_t level = 1;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(pmic->interrupt_isr_queue_, &level, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void Axp2101::InterruptTaskEntry(void* arg) {
    auto* pmic = static_cast<Axp2101*>(arg);
    if (pmic != nullptr) {
        pmic->InterruptTask();
    }
    vTaskDelete(nullptr);
}

void Axp2101::InterruptTask() {
    while (true) {
        uint32_t level = 0;
        if (xQueueReceive(interrupt_isr_queue_, &level, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        InterruptEvent event = {
            .level = GetInterruptLevel(),
            .irq_status = 0,
        };
        event.irq_status = getIrqStatus();
        LogInterruptStatus(event);
        if (event.irq_status != 0) {
            clearIrqStatus();
        }
        // This runs on the dedicated IRQ task (not in ISR context), so invoke the app callback
        // directly rather than re-dispatching through a service thread.
        if (interrupt_callback_) {
            interrupt_callback_(event);
        }
    }
}

esp_err_t Axp2101::ConfigureInterruptPin() {
    gpio_config_t config = {};
    config.intr_type = GPIO_INTR_ANYEDGE;
    config.mode = GPIO_MODE_INPUT;
    config.pin_bit_mask = 1ULL << interrupt_pin_;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&config), "Axp2101",
                        "failed to configure PMIC IRQ pin");
    // Install the shared GPIO ISR service; ESP_ERR_INVALID_STATE means another driver already
    // installed it, which is fine.
    const esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE("Axp2101", "failed to install GPIO ISR service: %s", esp_err_to_name(isr_err));
        return isr_err;
    }
    return gpio_isr_handler_add(interrupt_pin_, InterruptIsrHandler, this);
}
