#include "xpowers_axp2101_driver.h"

#include <esp_check.h>

Axp2101Driver::Axp2101Driver(i2c_master_bus_handle_t i2c_bus, uint8_t addr, uint32_t scl_speed_hz) {
    if (!begin(i2c_bus, addr, scl_speed_hz) || !init()) {
        log_e("Failed to initialize AXP2101 at address 0x%02X on %lu Hz I2C bus", addr,
              static_cast<unsigned long>(scl_speed_hz));
        ESP_ERROR_CHECK(ESP_FAIL);
    }
}

Axp2101Driver::~Axp2101Driver() {
    deinit();
}

void Axp2101Driver::setProtectedChannel(uint8_t channel) {
    protected_channel_mask_ |= (1u << channel);
}

void Axp2101Driver::clearProtectedChannel(uint8_t channel) {
    protected_channel_mask_ &= ~(1u << channel);
}

bool Axp2101Driver::getProtectedChannel(uint8_t channel) const {
    return (protected_channel_mask_ & (1u << channel)) != 0;
}

uint8_t Axp2101Driver::getChipModel() const {
    return chip_model_;
}

void Axp2101Driver::setChipModel(uint8_t model) {
    chip_model_ = model;
}

bool Axp2101Driver::init()
    {
        if (i2c_device_ == nullptr) {
            return false;
        }
        if (has_init_) {
            return initImpl();
        }
        has_init_ = true;
        if (!initImpl()) {
            has_init_ = false;
            return false;
        }
        return true;
    }

void Axp2101Driver::deinit()
    {
        has_init_ = false;
        end();
    }

uint16_t Axp2101Driver::status()
    {
        uint16_t status1 = readRegister(XPOWERS_AXP2101_STATUS1) & 0x1F;
        uint16_t status2 = readRegister(XPOWERS_AXP2101_STATUS2) & 0x1F;;
        return (status1 << 8) | (status2);
    }

bool Axp2101Driver::isVbusGood(void)
    {
        return  getRegisterBit(XPOWERS_AXP2101_STATUS1, 5);
    }

bool Axp2101Driver::getBatfetState(void)
    {
        return  getRegisterBit(XPOWERS_AXP2101_STATUS1, 4);
    }

bool Axp2101Driver::isBatteryConnect(void)
    {
        return  getRegisterBit(XPOWERS_AXP2101_STATUS1, 3);
    }

bool Axp2101Driver::isBatInActiveModeState(void)
    {
        return  getRegisterBit(XPOWERS_AXP2101_STATUS1, 2);
    }

bool Axp2101Driver::getThermalRegulationStatus(void)
    {
        return  getRegisterBit(XPOWERS_AXP2101_STATUS1, 1);
    }

bool Axp2101Driver::getCurrentLimitStatus(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_STATUS1, 0);
    }

bool Axp2101Driver::isCharging(void)
    {
        return (readRegister(XPOWERS_AXP2101_STATUS2) >> 5) == 0x01;
    }

bool Axp2101Driver::isDischarge(void)
    {
        return (readRegister(XPOWERS_AXP2101_STATUS2) >> 5) == 0x02;
    }

bool Axp2101Driver::isStandby(void)
    {
        return (readRegister(XPOWERS_AXP2101_STATUS2) >> 5) == 0x00;
    }

bool Axp2101Driver::isPowerOn(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_STATUS2, 4);
    }

bool Axp2101Driver::isPowerOff(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_STATUS2, 4);
    }

bool Axp2101Driver::isVbusIn(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_STATUS2, 3) == 0 && isVbusGood();
    }

xpowers_chg_status_t Axp2101Driver::getChargerStatus(void)
    {
        int val = readRegister(XPOWERS_AXP2101_STATUS2);
        if (val == -1)return XPOWERS_AXP2101_CHG_STOP_STATE;
        val &= 0x07;
        return (xpowers_chg_status_t)val;
    }

bool Axp2101Driver::writeDataBuffer(uint8_t *data, uint8_t size)
    {
        if (size > XPOWERS_AXP2101_DATA_BUFFER_SIZE)return false;
        return writeRegister(XPOWERS_AXP2101_DATA_BUFFER1, data, size);
    }

bool Axp2101Driver::readDataBuffer(uint8_t *data, uint8_t size)
    {
        if (size > XPOWERS_AXP2101_DATA_BUFFER_SIZE)return false;
        return readRegister(XPOWERS_AXP2101_DATA_BUFFER1, data, size);
    }

void Axp2101Driver::enableInternalDischarge(void)
    {
        setRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 5);
    }

void Axp2101Driver::disableInternalDischarge(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 5);
    }

void Axp2101Driver::enablePwrOkPinPullLow(void)
    {
        setRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 3);
    }

void Axp2101Driver::disablePwrOkPinPullLow(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 3);
    }

void Axp2101Driver::enablePwronShutPMIC(void)
    {
        setRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 2);
    }

void Axp2101Driver::disablePwronShutPMIC(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 2);
    }

void Axp2101Driver::reset(void)
    {
        setRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 1);
    }

void Axp2101Driver::shutdown(void)
    {
        setRegisterBit(XPOWERS_AXP2101_COMMON_CONFIG, 0);
    }

void Axp2101Driver::setBatfetDieOverTempLevel1(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_BATFET_CTRL);
        if (val == -1)return;
        val &= 0xF9;
        writeRegister(XPOWERS_AXP2101_BATFET_CTRL, val | (opt << 1));
    }

uint8_t Axp2101Driver::getBatfetDieOverTempLevel1(void)
    {
        return (readRegister(XPOWERS_AXP2101_BATFET_CTRL) & 0x06);
    }

void Axp2101Driver::enableBatfetDieOverTempDetect(void)
    {
        setRegisterBit(XPOWERS_AXP2101_BATFET_CTRL, 0);
    }

void Axp2101Driver::disableBatfetDieOverTempDetect(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_BATFET_CTRL, 0);
    }

void Axp2101Driver::setDieOverTempLevel1(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_DIE_TEMP_CTRL);
        if (val == -1)return;
        val &= 0xF9;
        writeRegister(XPOWERS_AXP2101_DIE_TEMP_CTRL, val | (opt << 1));
    }

uint8_t Axp2101Driver::getDieOverTempLevel1(void)
    {
        return (readRegister(XPOWERS_AXP2101_DIE_TEMP_CTRL) & 0x06);
    }

void Axp2101Driver::enableDieOverTempDetect(void)
    {
        setRegisterBit(XPOWERS_AXP2101_DIE_TEMP_CTRL, 0);
    }

void Axp2101Driver::disableDieOverTempDetect(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_DIE_TEMP_CTRL, 0);
    }

void Axp2101Driver::setLinearChargerVsysDpm(xpower_chg_dpm_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_MIN_SYS_VOL_CTRL);
        if (val == -1)return;
        val &= 0x8F;
        writeRegister(XPOWERS_AXP2101_MIN_SYS_VOL_CTRL, val | (opt << 4));
    }

uint8_t Axp2101Driver::getLinearChargerVsysDpm(void)
    {
        int val = readRegister(XPOWERS_AXP2101_MIN_SYS_VOL_CTRL);
        if (val == -1)return 0;
        val &= 0x70;
        return (val & 0x70) >> 4;
    }

void Axp2101Driver::setVbusVoltageLimit(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL);
        if (val == -1)return;
        val &= 0xF0;
        writeRegister(XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL, val | (opt & 0x0F));
    }

uint8_t Axp2101Driver::getVbusVoltageLimit(void)
    {
        return (readRegister(XPOWERS_AXP2101_INPUT_VOL_LIMIT_CTRL) & 0x0F);
    }

bool Axp2101Driver::setVbusCurrentLimit(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL);
        if (val == -1)return false;
        val &= 0xF8;
        return 0 == writeRegister(XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL, val | (opt & 0x07));
    }

uint8_t Axp2101Driver::getVbusCurrentLimit(void)
    {
        return (readRegister(XPOWERS_AXP2101_INPUT_CUR_LIMIT_CTRL) & 0x07);
    }

void Axp2101Driver::resetGauge(void)
    {
        setRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 3);
    }

void Axp2101Driver::resetGaugeBesides(void)
    {
        setRegisterBit(XPOWERS_AXP2101_RESET_FUEL_GAUGE, 2);
    }

void Axp2101Driver::enableGauge(void)
    {
        setRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 3);
    }

void Axp2101Driver::disableGauge(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 3);
    }

bool Axp2101Driver::enableButtonBatteryCharge(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 2);
    }

bool Axp2101Driver::disableButtonBatteryCharge(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 2);
    }

bool Axp2101Driver::isEnableButtonBatteryCharge()
    {
        return getRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 2);
    }

bool Axp2101Driver::setButtonBatteryChargeVoltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_BTN_VOL_STEPS) {
            log_e("Mistake ! Button battery charging step voltage is %u mV", XPOWERS_AXP2101_BTN_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_BTN_VOL_MIN) {
            log_e("Mistake ! The minimum charge termination voltage of the coin cell battery is %u mV", XPOWERS_AXP2101_BTN_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_BTN_VOL_MAX) {
            log_e("Mistake ! The minimum charge termination voltage of the coin cell battery is %u mV", XPOWERS_AXP2101_BTN_VOL_MAX);
            return false;
        }
        int val =  readRegister(XPOWERS_AXP2101_BTN_BAT_CHG_VOL_SET);
        if (val == -1)return 0;
        val  &= 0xF8;
        val |= (millivolt - XPOWERS_AXP2101_BTN_VOL_MIN) / XPOWERS_AXP2101_BTN_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_BTN_BAT_CHG_VOL_SET, val);
    }

uint16_t Axp2101Driver::getButtonBatteryVoltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_BTN_BAT_CHG_VOL_SET);
        if (val == -1)return 0;
        return (val & 0x07) * XPOWERS_AXP2101_BTN_VOL_STEPS + XPOWERS_AXP2101_BTN_VOL_MIN;
    }

void Axp2101Driver::enableCellbatteryCharge(void)
    {
        setRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 1);
    }

void Axp2101Driver::disableCellbatteryCharge(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 1);
    }

void Axp2101Driver::enableWatchdog(void)
    {
        setRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0);
        enableIRQ(XPOWERS_AXP2101_WDT_EXPIRE_IRQ);
    }

void Axp2101Driver::disableWatchdog(void)
    {
        disableIRQ(XPOWERS_AXP2101_WDT_EXPIRE_IRQ);
        clrRegisterBit(XPOWERS_AXP2101_CHARGE_GAUGE_WDT_CTRL, 0);
    }

void Axp2101Driver::setWatchdogConfig(xpowers_wdt_config_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_WDT_CTRL);
        if (val == -1)return;
        val &= 0xCF;
        writeRegister(XPOWERS_AXP2101_WDT_CTRL, val | (opt << 4));
    }

uint8_t Axp2101Driver::getWatchConfig(void)
    {
        return (readRegister(XPOWERS_AXP2101_WDT_CTRL) & 0x30) >> 4;
    }

void Axp2101Driver::clrWatchdog(void)
    {
        setRegisterBit(XPOWERS_AXP2101_WDT_CTRL, 3);
    }

void Axp2101Driver::setWatchdogTimeout(xpowers_wdt_timeout_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_WDT_CTRL);
        if (val == -1)return;
        val &= 0xF8;
        writeRegister(XPOWERS_AXP2101_WDT_CTRL, val | opt);
    }

uint8_t Axp2101Driver::getWatchdogTimerout(void)
    {
        return readRegister(XPOWERS_AXP2101_WDT_CTRL) & 0x07;
    }

void Axp2101Driver::setLowBatWarnThreshold(uint8_t percentage)
    {
        if (percentage < 5 || percentage > 20)return;
        int val = readRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET);
        if (val == -1)return;
        val &= 0x0F;
        writeRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET, val | ((percentage - 5) << 4));
    }

uint8_t Axp2101Driver::getLowBatWarnThreshold(void)
    {
        int val = readRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET);
        if (val == -1)return 0;
        val &= 0xF0;
        val >>= 4;
        return val;
    }

void Axp2101Driver::setLowBatShutdownThreshold(uint8_t opt)
    {
        if (opt > 15) {
            opt = 15;
        }
        int val = readRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET);
        if (val == -1)return;
        val &= 0xF0;
        writeRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET, val | opt);
    }

uint8_t Axp2101Driver::getLowBatShutdownThreshold(void)
    {
        return (readRegister(XPOWERS_AXP2101_LOW_BAT_WARN_SET) & 0x0F);
    }

bool Axp2101Driver::isPoweronAlwaysHighSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 5);
    }

bool Axp2101Driver::isBattInsertOnSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 4);
    }

bool Axp2101Driver::isBattNormalOnSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 3);
    }

bool Axp2101Driver::isVbusInsertOnSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 2);
    }

bool Axp2101Driver::isIrqLowOnSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 1);
    }

bool Axp2101Driver::isPwronLowOnSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWRON_STATUS, 0);
    }

xpower_power_on_source_t Axp2101Driver::getPowerOnSource()
    {
        int val = readRegister(XPOWERS_AXP2101_PWRON_STATUS);
        if (val == -1) return XPOWER_POWERON_SRC_UNKONW;
        return (xpower_power_on_source_t)val;
    }

bool Axp2101Driver::isOverTemperatureOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 7);
    }

bool Axp2101Driver::isDcOverVoltageOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 6);
    }

bool Axp2101Driver::isDcUnderVoltageOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 5);
    }

bool Axp2101Driver::isVbusOverVoltageOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 4);
    }

bool Axp2101Driver::isVsysUnderVoltageOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 3);
    }

bool Axp2101Driver::isPwronAlwaysLowOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 2);
    }

bool Axp2101Driver::isSwConfigOffSource()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 1);
    }

bool Axp2101Driver::isPwrSourcePullDown()
    {
        return getRegisterBit(XPOWERS_AXP2101_PWROFF_STATUS, 0);
    }

xpower_power_off_source_t Axp2101Driver::getPowerOffSource()
    {
        int val = readRegister(XPOWERS_AXP2101_PWROFF_STATUS);
        if (val == -1) return XPOWER_POWEROFF_SRC_UNKONW;
        return (xpower_power_off_source_t)val;
    }

void Axp2101Driver::enableOverTemperatureLevel2PowerOff()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 2);
    }

void Axp2101Driver::disableOverTemperaturePowerOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 2);
    }

void Axp2101Driver::enableLongPressShutdown()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 1);
    }

void Axp2101Driver::disableLongPressShutdown()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 1);
    }

void Axp2101Driver::setLongPressRestart()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 0);
    }

void Axp2101Driver::setLongPressPowerOFF()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROFF_EN, 0);
    }

void Axp2101Driver::enableDCHighVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 5);
    }

void Axp2101Driver::disableDCHighVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 5);
    }

void Axp2101Driver::enableDC5LowVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
    }

void Axp2101Driver::disableDC5LowVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
    }

void Axp2101Driver::enableDC4LowVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
    }

void Axp2101Driver::disableDC4LowVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
    }

void Axp2101Driver::enableDC3LowVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
    }

void Axp2101Driver::disableDC3LowVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
    }

void Axp2101Driver::enableDC2LowVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
    }

void Axp2101Driver::disableDC2LowVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
    }

void Axp2101Driver::enableDC1LowVoltageTurnOff()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
    }

void Axp2101Driver::disableDC1LowVoltageTurnOff()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
    }

bool Axp2101Driver::setSysPowerDownVoltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN) {
            log_e("Mistake ! The minimum settable voltage of VSYS is %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MAX) {
            log_e("Mistake ! The maximum settable voltage of VSYS is %u mV", XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MAX);
            return false;
        }
        int val = readRegister(XPOWERS_AXP2101_VOFF_SET);
        if (val == -1)return false;
        val &= 0xF8;
        return 0 == writeRegister(XPOWERS_AXP2101_VOFF_SET, val | ((millivolt - XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN) / XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS));
    }

uint16_t Axp2101Driver::getSysPowerDownVoltage(void)
    {
        int val = readRegister(XPOWERS_AXP2101_VOFF_SET);
        if (val == -1)return false;
        return (val & 0x07) * XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_STEPS + XPOWERS_AXP2101_VSYS_VOL_THRESHOLD_MIN;
    }

void Axp2101Driver::enablePwrOk()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 4);
    }

void Axp2101Driver::disablePwrOk()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 4);
    }

void Axp2101Driver::enablePowerOffDelay()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 3);
    }

void Axp2101Driver::disablePowerOffDelay()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 3);
    }

void Axp2101Driver::enablePowerSequence()
    {
        setRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 2);
    }

void Axp2101Driver::disablePowerSequence()
    {
        clrRegisterBit(XPOWERS_AXP2101_PWROK_SEQU_CTRL, 2);
    }

bool Axp2101Driver::setPwrOkDelay(xpower_pwrok_delay_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_PWROK_SEQU_CTRL);
        if (val == -1)return false;
        val &= 0xFC;
        return 0 == writeRegister(XPOWERS_AXP2101_PWROK_SEQU_CTRL, val | opt);
    }

xpower_pwrok_delay_t Axp2101Driver::getPwrOkDelay()
    {
        int val = readRegister(XPOWERS_AXP2101_PWROK_SEQU_CTRL);
        if (val == -1)return XPOWER_PWROK_DELAY_8MS;
        return (xpower_pwrok_delay_t)(val & 0x03);
    }

void Axp2101Driver::wakeupControl(xpowers_wakeup_t opt, bool enable)
    {
        int val = readRegister(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL);
        if (val == -1)return;
        enable ? (val |= opt) : (val &= (~opt));
        writeRegister(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL, val);
    }

bool Axp2101Driver::enableWakeup(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL, 1);
    }

bool Axp2101Driver::disableWakeup(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL, 1);
    }

bool Axp2101Driver::enableSleep(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL, 0);
    }

bool Axp2101Driver::disableSleep(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_SLEEP_WAKEUP_CTRL, 0);
    }

void Axp2101Driver::setIrqLevel(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return;
        val &= 0xFC;
        writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | (opt << 4));
    }

void Axp2101Driver::setOffLevel(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | (opt << 2));
    }

void Axp2101Driver::setOnLevel(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | opt);
    }

void Axp2101Driver::setDc4FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | ((opt & 0x3) << 6));
    }

void Axp2101Driver::setDc3FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | ((opt & 0x3) << 4));
    }

void Axp2101Driver::setDc2FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | ((opt & 0x3) << 2));
    }

void Axp2101Driver::setDc1FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | (opt & 0x3));
    }

void Axp2101Driver::setAldo3FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | ((opt & 0x3) << 6));
    }

void Axp2101Driver::setAldo2FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | ((opt & 0x3) << 4));
    }

void Axp2101Driver::setAldo1FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | ((opt & 0x3) << 2));
    }

void Axp2101Driver::setDc5FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | (opt & 0x3));
    }

void Axp2101Driver::setCpuldoFastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | ((opt & 0x3) << 6));
    }

void Axp2101Driver::setBldo2FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | ((opt & 0x3) << 4));
    }

void Axp2101Driver::setBldo1FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | ((opt & 0x3) << 2));
    }

void Axp2101Driver::setAldo4FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | (opt & 0x3));
    }

void Axp2101Driver::setDldo2FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val | ((opt & 0x3) << 2));
    }

void Axp2101Driver::setDldo1FastStartSequence(xpower_start_sequence_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val | (opt & 0x3));
    }

void Axp2101Driver::setFastPowerOnLevel(xpowers_fast_on_opt_t opt, xpower_start_sequence_t seq_level)
    {
        uint8_t val = 0;
        switch (opt) {
        case XPOWERS_AXP2101_FAST_DCDC1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | seq_level);
            break;
        case XPOWERS_AXP2101_FAST_DCDC2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | (seq_level << 2));
            break;
        case XPOWERS_AXP2101_FAST_DCDC3:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | (seq_level << 4));
            break;
        case XPOWERS_AXP2101_FAST_DCDC4:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val | (seq_level << 6));
            break;
        case XPOWERS_AXP2101_FAST_DCDC5:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | seq_level);
            break;
        case XPOWERS_AXP2101_FAST_ALDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | (seq_level << 2));
            break;
        case XPOWERS_AXP2101_FAST_ALDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | (seq_level << 4));
            break;
        case XPOWERS_AXP2101_FAST_ALDO3:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val | (seq_level << 6));
            break;
        case XPOWERS_AXP2101_FAST_ALDO4:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | seq_level);
            break;
        case XPOWERS_AXP2101_FAST_BLDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | (seq_level << 2));
            break;
        case XPOWERS_AXP2101_FAST_BLDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | (seq_level << 4));
            break;
        case XPOWERS_AXP2101_FAST_CPUSLDO:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val | (seq_level << 6));
            break;
        case XPOWERS_AXP2101_FAST_DLDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val | seq_level);
            break;
        case XPOWERS_AXP2101_FAST_DLDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val | (seq_level << 2));
            break;
        default:
            break;
        }
    }

void Axp2101Driver::disableFastPowerOn(xpowers_fast_on_opt_t opt)
    {
        uint8_t val = 0;
        switch (opt) {
        case XPOWERS_AXP2101_FAST_DCDC1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val & 0xFC);
            break;
        case XPOWERS_AXP2101_FAST_DCDC2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val & 0xF3);
            break;
        case XPOWERS_AXP2101_FAST_DCDC3:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val & 0xCF);
            break;
        case XPOWERS_AXP2101_FAST_DCDC4:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET0);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET0, val & 0x3F);
            break;
        case XPOWERS_AXP2101_FAST_DCDC5:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val & 0xFC);
            break;
        case XPOWERS_AXP2101_FAST_ALDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val & 0xF3);
            break;
        case XPOWERS_AXP2101_FAST_ALDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val & 0xCF);
            break;
        case XPOWERS_AXP2101_FAST_ALDO3:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET1);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET1, val & 0x3F);
            break;
        case XPOWERS_AXP2101_FAST_ALDO4:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val & 0xFC);
            break;
        case XPOWERS_AXP2101_FAST_BLDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val & 0xF3);
            break;
        case XPOWERS_AXP2101_FAST_BLDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val & 0xCF);
            break;
        case XPOWERS_AXP2101_FAST_CPUSLDO:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_SET2);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_SET2, val & 0x3F);
            break;
        case XPOWERS_AXP2101_FAST_DLDO1:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val & 0xFC);
            break;
        case XPOWERS_AXP2101_FAST_DLDO2:
            val = readRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL);
            writeRegister(XPOWERS_AXP2101_FAST_PWRON_CTRL, val & 0xF3);
            break;
        default:
            break;
        }
    }

void Axp2101Driver::enableFastPowerOn(void)
    {
        setRegisterBit(XPOWERS_AXP2101_FAST_PWRON_CTRL, 7);
    }

void Axp2101Driver::disableFastPowerOn(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_FAST_PWRON_CTRL, 7);
    }

void Axp2101Driver::enableFastWakeup(void)
    {
        setRegisterBit(XPOWERS_AXP2101_FAST_PWRON_CTRL, 6);
    }

void Axp2101Driver::disableFastWakeup(void)
    {
        clrRegisterBit(XPOWERS_AXP2101_FAST_PWRON_CTRL, 6);
    }

void Axp2101Driver::setDCHighVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 5) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 5);
    }

bool Axp2101Driver::getDCHighVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 5);
    }

void Axp2101Driver::setDcUVPDebounceTime(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL);
        val &= 0xFC;
        writeRegister(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, val | opt);
    }

void Axp2101Driver::settDC1WorkModeToPwm(uint8_t enable)
    {
        enable ?
        setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 2)
        : clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 2);
    }

void Axp2101Driver::settDC2WorkModeToPwm(uint8_t enable)
    {
        enable ? setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 3)
        : clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 3);
    }

void Axp2101Driver::settDC3WorkModeToPwm(uint8_t enable)
    {
        enable ?
        setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 4)
        : clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 4);
    }

void Axp2101Driver::settDC4WorkModeToPwm( uint8_t enable)
    {
        enable ?
        setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 5)
        :  clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 5);
    }

void Axp2101Driver::setDCFreqSpreadRange(uint8_t opt)
    {
        opt ?
        setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 6)
        :  clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 6);
    }

void Axp2101Driver::setDCFreqSpreadRangeEn(bool en)
    {
        en ?
        setRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 7)
        :  clrRegisterBit(XPOWERS_AXP2101_DC_FORCE_PWM_CTRL, 7);
    }

void Axp2101Driver::enableCCM()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 6);
    }

void Axp2101Driver::disableCCM()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 6);
    }

bool Axp2101Driver::isenableCCM()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 6);
    }

void Axp2101Driver::setDVMRamp(uint8_t opt)
    {
        if (opt > 2)return;
        opt == 0 ? clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 5) : setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 5);
    }

bool Axp2101Driver::isEnableDC1(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
    }

bool Axp2101Driver::enableDC1(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
    }

bool Axp2101Driver::disableDC1(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 0);
    }

bool Axp2101Driver::setDC1Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_DCDC1_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DCDC1_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_DCDC1_VOL_MIN) {
            log_e("Mistake ! DC1 minimum voltage is %u mV", XPOWERS_AXP2101_DCDC1_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_DCDC1_VOL_MAX) {
            log_e("Mistake ! DC1 maximum voltage is %u mV", XPOWERS_AXP2101_DCDC1_VOL_MAX);
            return false;
        }
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL0_CTRL, (millivolt - XPOWERS_AXP2101_DCDC1_VOL_MIN) / XPOWERS_AXP2101_DCDC1_VOL_STEPS);
    }

uint16_t Axp2101Driver::getDC1Voltage(void)
    {
        return (readRegister(XPOWERS_AXP2101_DC_VOL0_CTRL) & 0x1F) * XPOWERS_AXP2101_DCDC1_VOL_STEPS + XPOWERS_AXP2101_DCDC1_VOL_MIN;
    }

void Axp2101Driver::setDC1LowVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
    }

bool Axp2101Driver::getDC1LowVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 0);
    }

bool Axp2101Driver::isEnableDC2(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
    }

bool Axp2101Driver::enableDC2(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
    }

bool Axp2101Driver::disableDC2(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 1);
    }

bool Axp2101Driver::setDC2Voltage(uint16_t millivolt)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL1_CTRL);
        if (val == -1)return 0;
        val &= 0x80;
        if (millivolt >= XPOWERS_AXP2101_DCDC2_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC2_VOL1_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC2_VOL_STEPS1) {
                log_e("Mistake !  The steps is must %umV", XPOWERS_AXP2101_DCDC2_VOL_STEPS1);
                return false;
            }
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL1_CTRL, val | (millivolt - XPOWERS_AXP2101_DCDC2_VOL1_MIN) / XPOWERS_AXP2101_DCDC2_VOL_STEPS1);
        } else if (millivolt >= XPOWERS_AXP2101_DCDC2_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC2_VOL2_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC2_VOL_STEPS2) {
                log_e("Mistake !  The steps is must %umV", XPOWERS_AXP2101_DCDC2_VOL_STEPS2);
                return false;
            }
            val |= (((millivolt - XPOWERS_AXP2101_DCDC2_VOL2_MIN) / XPOWERS_AXP2101_DCDC2_VOL_STEPS2) + XPOWERS_AXP2101_DCDC2_VOL_STEPS2_BASE);
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL1_CTRL, val);
        }
        return false;
    }

uint16_t Axp2101Driver::getDC2Voltage(void)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL1_CTRL);
        if (val ==  -1)return 0;
        val &= 0x7F;
        if (val < XPOWERS_AXP2101_DCDC2_VOL_STEPS2_BASE) {
            return (val  * XPOWERS_AXP2101_DCDC2_VOL_STEPS1) +  XPOWERS_AXP2101_DCDC2_VOL1_MIN;
        } else  {
            return (val  * XPOWERS_AXP2101_DCDC2_VOL_STEPS2) - 200;
        }
        return 0;
    }

uint8_t Axp2101Driver::getDC2WorkMode(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DCDC2_VOL_STEPS2, 7);
    }

void Axp2101Driver::setDC2LowVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
    }

bool Axp2101Driver::getDC2LowVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 1);
    }

bool Axp2101Driver::isEnableDC3(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
    }

bool Axp2101Driver::enableDC3(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
    }

bool Axp2101Driver::disableDC3(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 2);
    }

bool Axp2101Driver::setDC3Voltage(uint16_t millivolt)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL2_CTRL);
        if (val == -1)return false;
        val &= 0x80;
        if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL1_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS1) {
                log_e("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS1);
                return false;
            }
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL, val | (millivolt - XPOWERS_AXP2101_DCDC3_VOL_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS1);
        } else if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL2_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS2) {
                log_e("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS2);
                return false;
            }
            val |= (((millivolt - XPOWERS_AXP2101_DCDC3_VOL2_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS2) + XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE);
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL, val);
        } else if (millivolt >= XPOWERS_AXP2101_DCDC3_VOL3_MIN && millivolt <= XPOWERS_AXP2101_DCDC3_VOL3_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC3_VOL_STEPS3) {
                log_e("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC3_VOL_STEPS3);
                return false;
            }
            val |= (((millivolt - XPOWERS_AXP2101_DCDC3_VOL3_MIN) / XPOWERS_AXP2101_DCDC3_VOL_STEPS3) + XPOWERS_AXP2101_DCDC3_VOL_STEPS3_BASE);
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL2_CTRL, val);
        }
        return false;
    }

uint16_t Axp2101Driver::getDC3Voltage(void)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL2_CTRL) & 0x7F;
        if (val == -1)
            return 0;
        if (val < XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE) {
            return (val  * XPOWERS_AXP2101_DCDC3_VOL_STEPS1) +  XPOWERS_AXP2101_DCDC3_VOL_MIN;
        } else if (val >= XPOWERS_AXP2101_DCDC3_VOL_STEPS2_BASE && val < XPOWERS_AXP2101_DCDC3_VOL_STEPS3_BASE) {
            return (val  * XPOWERS_AXP2101_DCDC3_VOL_STEPS2) - 200;
        } else  {
            return (val  * XPOWERS_AXP2101_DCDC3_VOL_STEPS3)  - 7200;
        }
        return 0;
    }

uint8_t Axp2101Driver::getDC3WorkMode(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_VOL2_CTRL, 7);
    }

void Axp2101Driver::setDC3LowVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
    }

bool Axp2101Driver::getDC3LowVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 2);
    }

bool Axp2101Driver::isEnableDC4(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
    }

bool Axp2101Driver::enableDC4(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
    }

bool Axp2101Driver::disableDC4(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 3);
    }

bool Axp2101Driver::setDC4Voltage(uint16_t millivolt)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL3_CTRL);
        if (val == -1)return false;
        val &= 0x80;
        if (millivolt >= XPOWERS_AXP2101_DCDC4_VOL1_MIN && millivolt <= XPOWERS_AXP2101_DCDC4_VOL1_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC4_VOL_STEPS1) {
                log_e("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC4_VOL_STEPS1);
                return false;
            }
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL3_CTRL, val | (millivolt - XPOWERS_AXP2101_DCDC4_VOL1_MIN) / XPOWERS_AXP2101_DCDC4_VOL_STEPS1);

        } else if (millivolt >= XPOWERS_AXP2101_DCDC4_VOL2_MIN && millivolt <= XPOWERS_AXP2101_DCDC4_VOL2_MAX) {
            if (millivolt % XPOWERS_AXP2101_DCDC4_VOL_STEPS2) {
                log_e("Mistake ! The steps is must %umV", XPOWERS_AXP2101_DCDC4_VOL_STEPS2);
                return false;
            }
            val |= (((millivolt - XPOWERS_AXP2101_DCDC4_VOL2_MIN) / XPOWERS_AXP2101_DCDC4_VOL_STEPS2) + XPOWERS_AXP2101_DCDC4_VOL_STEPS2_BASE);
            return  0 == writeRegister(XPOWERS_AXP2101_DC_VOL3_CTRL, val);

        }
        return false;
    }

uint16_t Axp2101Driver::getDC4Voltage(void)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL3_CTRL);
        if (val == -1)return 0;
        val &= 0x7F;
        if (val < XPOWERS_AXP2101_DCDC4_VOL_STEPS2_BASE) {
            return (val  * XPOWERS_AXP2101_DCDC4_VOL_STEPS1) +  XPOWERS_AXP2101_DCDC4_VOL1_MIN;
        } else  {
            return (val  * XPOWERS_AXP2101_DCDC4_VOL_STEPS2) - 200;
        }
        return 0;
    }

void Axp2101Driver::setDC4LowVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
    }

bool Axp2101Driver::getDC4LowVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 3);
    }

bool Axp2101Driver::isEnableDC5(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
    }

bool Axp2101Driver::enableDC5(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
    }

bool Axp2101Driver::disableDC5(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_DC_ONOFF_DVM_CTRL, 4);
    }

bool Axp2101Driver::setDC5Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_DCDC5_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DCDC5_VOL_STEPS);
            return false;
        }
        if (millivolt != XPOWERS_AXP2101_DCDC5_VOL_1200MV && millivolt < XPOWERS_AXP2101_DCDC5_VOL_MIN) {
            log_e("Mistake ! DC5 minimum voltage is %umV ,%umV", XPOWERS_AXP2101_DCDC5_VOL_1200MV, XPOWERS_AXP2101_DCDC5_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_DCDC5_VOL_MAX) {
            log_e("Mistake ! DC5 maximum voltage is %umV", XPOWERS_AXP2101_DCDC5_VOL_MAX);
            return false;
        }

        int val =  readRegister(XPOWERS_AXP2101_DC_VOL4_CTRL);
        if (val == -1)return false;
        val &= 0xE0;
        if (millivolt == XPOWERS_AXP2101_DCDC5_VOL_1200MV) {
            return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL4_CTRL, val | XPOWERS_AXP2101_DCDC5_VOL_VAL);
        }
        val |= (millivolt - XPOWERS_AXP2101_DCDC5_VOL_MIN) / XPOWERS_AXP2101_DCDC5_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_DC_VOL4_CTRL, val);
    }

uint16_t Axp2101Driver::getDC5Voltage(void)
    {
        int val = readRegister(XPOWERS_AXP2101_DC_VOL4_CTRL) ;
        if (val == -1)return 0;
        val &= 0x1F;
        if (val == XPOWERS_AXP2101_DCDC5_VOL_VAL)return XPOWERS_AXP2101_DCDC5_VOL_1200MV;
        return  (val * XPOWERS_AXP2101_DCDC5_VOL_STEPS) + XPOWERS_AXP2101_DCDC5_VOL_MIN;
    }

bool Axp2101Driver::isDC5FreqCompensationEn(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
    }

void Axp2101Driver::enableDC5FreqCompensation()
    {
        setRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
    }

void Axp2101Driver::disableFreqCompensation()
    {
        clrRegisterBit(XPOWERS_AXP2101_DC_VOL4_CTRL, 5);
    }

void Axp2101Driver::setDC5LowVoltagePowerDown(bool en)
    {
        en ? setRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4) : clrRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
    }

bool Axp2101Driver::getDC5LowVoltagePowerDownEn()
    {
        return getRegisterBit(XPOWERS_AXP2101_DC_OVP_UVP_CTRL, 4);
    }

bool Axp2101Driver::isEnableALDO1(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0);
    }

bool Axp2101Driver::enableALDO1(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0);
    }

bool Axp2101Driver::disableALDO1(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 0);
    }

bool Axp2101Driver::setALDO1Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_ALDO1_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO1_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_ALDO1_VOL_MIN) {
            log_e("Mistake ! ALDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO1_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_ALDO1_VOL_MAX) {
            log_e("Mistake ! ALDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO1_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_ALDO1_VOL_MIN) / XPOWERS_AXP2101_ALDO1_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL, val);
    }

uint16_t Axp2101Driver::getALDO1Voltage(void)
    {
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL0_CTRL) & 0x1F;
        return val * XPOWERS_AXP2101_ALDO1_VOL_STEPS + XPOWERS_AXP2101_ALDO1_VOL_MIN;
    }

bool Axp2101Driver::isEnableALDO2(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
    }

bool Axp2101Driver::enableALDO2(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
    }

bool Axp2101Driver::disableALDO2(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 1);
    }

bool Axp2101Driver::setALDO2Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_ALDO2_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO2_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_ALDO2_VOL_MIN) {
            log_e("Mistake ! ALDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO2_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_ALDO2_VOL_MAX) {
            log_e("Mistake ! ALDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO2_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_ALDO2_VOL_MIN) / XPOWERS_AXP2101_ALDO2_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL, val);
    }

uint16_t Axp2101Driver::getALDO2Voltage(void)
    {
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL1_CTRL) & 0x1F;
        return val * XPOWERS_AXP2101_ALDO2_VOL_STEPS + XPOWERS_AXP2101_ALDO2_VOL_MIN;
    }

bool Axp2101Driver::isEnableALDO3(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
    }

bool Axp2101Driver::enableALDO3(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
    }

bool Axp2101Driver::disableALDO3(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 2);
    }

bool Axp2101Driver::setALDO3Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_ALDO3_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO3_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_ALDO3_VOL_MIN) {
            log_e("Mistake ! ALDO3 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO3_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_ALDO3_VOL_MAX) {
            log_e("Mistake ! ALDO3 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO3_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_ALDO3_VOL_MIN) / XPOWERS_AXP2101_ALDO3_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL, val);
    }

uint16_t Axp2101Driver::getALDO3Voltage(void)
    {
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL2_CTRL) & 0x1F;
        return val * XPOWERS_AXP2101_ALDO3_VOL_STEPS + XPOWERS_AXP2101_ALDO3_VOL_MIN;
    }

bool Axp2101Driver::isEnableALDO4(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
    }

bool Axp2101Driver::enableALDO4(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
    }

bool Axp2101Driver::disableALDO4(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 3);
    }

bool Axp2101Driver::setALDO4Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_ALDO4_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_ALDO4_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_ALDO4_VOL_MIN) {
            log_e("Mistake ! ALDO4 minimum output voltage is  %umV", XPOWERS_AXP2101_ALDO4_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_ALDO4_VOL_MAX) {
            log_e("Mistake ! ALDO4 maximum output voltage is  %umV", XPOWERS_AXP2101_ALDO4_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_ALDO4_VOL_MIN) / XPOWERS_AXP2101_ALDO4_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL, val);
    }

uint16_t Axp2101Driver::getALDO4Voltage(void)
    {
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL3_CTRL) & 0x1F;
        return val * XPOWERS_AXP2101_ALDO4_VOL_STEPS + XPOWERS_AXP2101_ALDO4_VOL_MIN;
    }

bool Axp2101Driver::isEnableBLDO1(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
    }

bool Axp2101Driver::enableBLDO1(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
    }

bool Axp2101Driver::disableBLDO1(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 4);
    }

bool Axp2101Driver::setBLDO1Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_BLDO1_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_BLDO1_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_BLDO1_VOL_MIN) {
            log_e("Mistake ! BLDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_BLDO1_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_BLDO1_VOL_MAX) {
            log_e("Mistake ! BLDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_BLDO1_VOL_MAX);
            return false;
        }
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL);
        if (val == -1)return  false;
        val &= 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_BLDO1_VOL_MIN) / XPOWERS_AXP2101_BLDO1_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL, val);
    }

uint16_t Axp2101Driver::getBLDO1Voltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL4_CTRL);
        if (val == -1)return 0;
        val &= 0x1F;
        return val * XPOWERS_AXP2101_BLDO1_VOL_STEPS + XPOWERS_AXP2101_BLDO1_VOL_MIN;
    }

bool Axp2101Driver::isEnableBLDO2(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
    }

bool Axp2101Driver::enableBLDO2(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
    }

bool Axp2101Driver::disableBLDO2(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 5);
    }

bool Axp2101Driver::setBLDO2Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_BLDO2_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_BLDO2_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_BLDO2_VOL_MIN) {
            log_e("Mistake ! BLDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_BLDO2_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_BLDO2_VOL_MAX) {
            log_e("Mistake ! BLDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_BLDO2_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_BLDO2_VOL_MIN) / XPOWERS_AXP2101_BLDO2_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL, val);
    }

uint16_t Axp2101Driver::getBLDO2Voltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL5_CTRL);
        if (val == -1)return 0;
        val &= 0x1F;
        return val * XPOWERS_AXP2101_BLDO2_VOL_STEPS + XPOWERS_AXP2101_BLDO2_VOL_MIN;
    }

bool Axp2101Driver::isEnableCPUSLDO(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
    }

bool Axp2101Driver::enableCPUSLDO(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
    }

bool Axp2101Driver::disableCPUSLDO(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 6);
    }

bool Axp2101Driver::setCPUSLDOVoltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_CPUSLDO_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_CPUSLDO_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_CPUSLDO_VOL_MIN) {
            log_e("Mistake ! CPULDO minimum output voltage is  %umV", XPOWERS_AXP2101_CPUSLDO_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_CPUSLDO_VOL_MAX) {
            log_e("Mistake ! CPULDO maximum output voltage is  %umV", XPOWERS_AXP2101_CPUSLDO_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_CPUSLDO_VOL_MIN) / XPOWERS_AXP2101_CPUSLDO_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL, val);
    }

uint16_t Axp2101Driver::getCPUSLDOVoltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL6_CTRL);
        if (val == -1)return 0;
        val &= 0x1F;
        return val * XPOWERS_AXP2101_CPUSLDO_VOL_STEPS + XPOWERS_AXP2101_CPUSLDO_VOL_MIN;
    }

bool Axp2101Driver::isEnableDLDO1(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
    }

bool Axp2101Driver::enableDLDO1(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
    }

bool Axp2101Driver::disableDLDO1(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL0, 7);
    }

bool Axp2101Driver::setDLDO1Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_DLDO1_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DLDO1_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_DLDO1_VOL_MIN) {
            log_e("Mistake ! DLDO1 minimum output voltage is  %umV", XPOWERS_AXP2101_DLDO1_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_DLDO1_VOL_MAX) {
            log_e("Mistake ! DLDO1 maximum output voltage is  %umV", XPOWERS_AXP2101_DLDO1_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_DLDO1_VOL_MIN) / XPOWERS_AXP2101_DLDO1_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL, val);
    }

uint16_t Axp2101Driver::getDLDO1Voltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL7_CTRL);
        if (val == -1)return 0;
        val &= 0x1F;
        return val * XPOWERS_AXP2101_DLDO1_VOL_STEPS + XPOWERS_AXP2101_DLDO1_VOL_MIN;
    }

bool Axp2101Driver::isEnableDLDO2(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
    }

bool Axp2101Driver::enableDLDO2(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
    }

bool Axp2101Driver::disableDLDO2(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_LDO_ONOFF_CTRL1, 0);
    }

bool Axp2101Driver::setDLDO2Voltage(uint16_t millivolt)
    {
        if (millivolt % XPOWERS_AXP2101_DLDO2_VOL_STEPS) {
            log_e("Mistake ! The steps is must %u mV", XPOWERS_AXP2101_DLDO2_VOL_STEPS);
            return false;
        }
        if (millivolt < XPOWERS_AXP2101_DLDO2_VOL_MIN) {
            log_e("Mistake ! DLDO2 minimum output voltage is  %umV", XPOWERS_AXP2101_DLDO2_VOL_MIN);
            return false;
        } else if (millivolt > XPOWERS_AXP2101_DLDO2_VOL_MAX) {
            log_e("Mistake ! DLDO2 maximum output voltage is  %umV", XPOWERS_AXP2101_DLDO2_VOL_MAX);
            return false;
        }
        uint16_t val =  readRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL) & 0xE0;
        val |= (millivolt - XPOWERS_AXP2101_DLDO2_VOL_MIN) / XPOWERS_AXP2101_DLDO2_VOL_STEPS;
        return 0 == writeRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL, val);
    }

uint16_t Axp2101Driver::getDLDO2Voltage(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_LDO_VOL8_CTRL);
        if (val == -1)return 0;
        val &= 0x1F;
        return val * XPOWERS_AXP2101_DLDO2_VOL_STEPS + XPOWERS_AXP2101_DLDO2_VOL_MIN;
    }

void Axp2101Driver::setIrqLevelTime(xpowers_irq_time_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return;
        val &= 0xCF;
        writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | (opt << 4));
    }

xpowers_irq_time_t Axp2101Driver::getIrqLevelTime(void)
    {
        return (xpowers_irq_time_t)((readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL) & 0x30) >> 4);
    }

bool Axp2101Driver::setPowerKeyPressOnTime(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return false;
        val  &= 0xFC;
        return 0 ==  writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | opt);
    }

uint8_t Axp2101Driver::getPowerKeyPressOnTime(void)
    {
        int val =  readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return 0;
        return (val & 0x03) ;
    }

bool Axp2101Driver::setPowerKeyPressOffTime(uint8_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL);
        if (val == -1)return false;
        val  &= 0xF3;
        return 0 == writeRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL, val | (opt << 2));
    }

uint8_t Axp2101Driver::getPowerKeyPressOffTime(void)
    {
        return ((readRegister(XPOWERS_AXP2101_IRQ_OFF_ON_LEVEL_CTRL) & 0x0C) >> 2);
    }

bool Axp2101Driver::enableGeneralAdcChannel(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 5);
    }

bool Axp2101Driver::disableGeneralAdcChannel(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 5);
    }

bool Axp2101Driver::enableTemperatureMeasure(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 4);
    }

bool Axp2101Driver::disableTemperatureMeasure(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 4);
    }

float Axp2101Driver::getTemperature(void)
    {
        uint16_t raw = readRegisterH6L8(XPOWERS_AXP2101_ADC_DATA_RELUST8, XPOWERS_AXP2101_ADC_DATA_RELUST9);
        return XPOWERS_AXP2101_CONVERSION(raw);
    }

bool Axp2101Driver::enableSystemVoltageMeasure(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 3);
    }

bool Axp2101Driver::disableSystemVoltageMeasure(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 3);
    }

uint16_t Axp2101Driver::getSystemVoltage(void)
    {
        return readRegisterH6L8(XPOWERS_AXP2101_ADC_DATA_RELUST6, XPOWERS_AXP2101_ADC_DATA_RELUST7);
    }

bool Axp2101Driver::enableVbusVoltageMeasure(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 2);
    }

bool Axp2101Driver::disableVbusVoltageMeasure(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 2);
    }

uint16_t Axp2101Driver::getVbusVoltage(void)
    {
        if (!isVbusIn()) {
            return 0;
        }
        return readRegisterH6L8(XPOWERS_AXP2101_ADC_DATA_RELUST4, XPOWERS_AXP2101_ADC_DATA_RELUST5);
    }

bool Axp2101Driver::enableTSPinMeasure(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 1);
    }

bool Axp2101Driver::disableTSPinMeasure(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 1);
    }

bool Axp2101Driver::enableTSPinLowFreqSample(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 7);
    }

bool Axp2101Driver::disableTSPinLowFreqSample(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_DATA_RELUST2, 7);
    }

uint16_t Axp2101Driver::getTsTemperature(void)
    {
        return readRegisterH6L8(XPOWERS_AXP2101_ADC_DATA_RELUST2, XPOWERS_AXP2101_ADC_DATA_RELUST3);
    }

bool Axp2101Driver::enableBattVoltageMeasure(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 0);
    }

bool Axp2101Driver::disableBattVoltageMeasure(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_ADC_CHANNEL_CTRL, 0);
    }

bool Axp2101Driver::enableBattDetection(void)
    {
        return setRegisterBit(XPOWERS_AXP2101_BAT_DET_CTRL, 0);
    }

bool Axp2101Driver::disableBattDetection(void)
    {
        return clrRegisterBit(XPOWERS_AXP2101_BAT_DET_CTRL, 0);
    }

uint16_t Axp2101Driver::getBattVoltage(void)
    {
        if (!isBatteryConnect()) {
            return 0;
        }
        return readRegisterH5L8(XPOWERS_AXP2101_ADC_DATA_RELUST0, XPOWERS_AXP2101_ADC_DATA_RELUST1);
    }

int Axp2101Driver::getBatteryPercent(void)
    {
        if (!isBatteryConnect()) {
            return -1;
        }
        return readRegister(XPOWERS_AXP2101_BAT_PERCENT_DATA);
    }

void Axp2101Driver::setChargingLedMode(uint8_t mode)
    {
        int val;
        switch (mode) {
        case XPOWERS_CHG_LED_OFF:
        // clrRegisterBit(XPOWERS_AXP2101_CHGLED_SET_CTRL, 0);
        // break;
        case XPOWERS_CHG_LED_BLINK_1HZ:
        case XPOWERS_CHG_LED_BLINK_4HZ:
        case XPOWERS_CHG_LED_ON:
            val = readRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL);
            if (val == -1)return;
            val &= 0xC8;
            val |= 0x05;    //use manual ctrl
            val |= (mode << 4);
            writeRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL, val);
            break;
        case XPOWERS_CHG_LED_CTRL_CHG:
            val = readRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL);
            if (val == -1)return;
            val &= 0xF9;
            writeRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL, val | 0x01); // use type A mode
            // writeRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL, val | 0x02); // use type B mode
            break;
        default:
            break;
        }
    }

uint8_t Axp2101Driver::getChargingLedMode()
    {
        int val = readRegister(XPOWERS_AXP2101_CHGLED_SET_CTRL);
        if (val == -1)return XPOWERS_CHG_LED_OFF;
        val >>= 1;
        if ((val & 0x02) == 0x02) {
            val >>= 4;
            return val & 0x03;
        }
        return XPOWERS_CHG_LED_CTRL_CHG;
    }

void Axp2101Driver::setPrechargeCurr(xpowers_prechg_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_IPRECHG_SET);
        if (val == -1)return;
        val &= 0xFC;
        writeRegister(XPOWERS_AXP2101_IPRECHG_SET, val | opt);
    }

xpowers_prechg_t Axp2101Driver::getPrechargeCurr(void)
    {
        return (xpowers_prechg_t)(readRegister(XPOWERS_AXP2101_IPRECHG_SET) & 0x03);
    }

bool Axp2101Driver::setChargerConstantCurr(uint8_t opt)
    {
        if (opt > XPOWERS_AXP2101_CHG_CUR_1000MA)return false;
        int val = readRegister(XPOWERS_AXP2101_ICC_CHG_SET);
        if (val == -1)return false;
        val &= 0xE0;
        return 0 == writeRegister(XPOWERS_AXP2101_ICC_CHG_SET, val | opt);
    }

uint8_t Axp2101Driver::getChargerConstantCurr(void)
    {
        int val = readRegister(XPOWERS_AXP2101_ICC_CHG_SET);
        if (val == -1)return 0;
        return val & 0x1F;
    }

void Axp2101Driver::setChargerTerminationCurr(xpowers_axp2101_chg_iterm_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL);
        if (val == -1)return;
        val &= 0xF0;
        writeRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, val | opt);
    }

xpowers_axp2101_chg_iterm_t Axp2101Driver::getChargerTerminationCurr(void)
    {
        return (xpowers_axp2101_chg_iterm_t)(readRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL) & 0x0F);
    }

void Axp2101Driver::enableChargerTerminationLimit(void)
    {
        int val = readRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, val | 0x10);
    }

void Axp2101Driver::disableChargerTerminationLimit(void)
    {
        int val = readRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL);
        if (val == -1)return;
        writeRegister(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, val & 0xEF);
    }

bool Axp2101Driver::isChargerTerminationLimit(void)
    {
        return getRegisterBit(XPOWERS_AXP2101_ITERM_CHG_SET_CTRL, 4);
    }

bool Axp2101Driver::setChargeTargetVoltage(uint8_t opt)
    {
        if (opt >= XPOWERS_AXP2101_CHG_VOL_MAX)return false;
        int val = readRegister(XPOWERS_AXP2101_CV_CHG_VOL_SET);
        if (val == -1)return false;
        val &= 0xF8;
        return 0 == writeRegister(XPOWERS_AXP2101_CV_CHG_VOL_SET, val | opt);
    }

uint8_t Axp2101Driver::getChargeTargetVoltage(void)
    {
        return (readRegister(XPOWERS_AXP2101_CV_CHG_VOL_SET) & 0x07);
    }

void Axp2101Driver::setThermaThreshold(xpowers_thermal_t opt)
    {
        int val = readRegister(XPOWERS_AXP2101_THE_REGU_THRES_SET);
        if (val == -1)return;
        val &= 0xFC;
        writeRegister(XPOWERS_AXP2101_THE_REGU_THRES_SET, val | opt);
    }

xpowers_thermal_t Axp2101Driver::getThermaThreshold(void)
    {
        return (xpowers_thermal_t)(readRegister(XPOWERS_AXP2101_THE_REGU_THRES_SET) & 0x03);
    }

uint8_t Axp2101Driver::getBatteryParameter()
    {
        return  readRegister(XPOWERS_AXP2101_BAT_PARAME);
    }

void Axp2101Driver::fuelGaugeControl(bool writeROM, bool enable)
    {
        if (writeROM) {
            clrRegisterBit(XPOWERS_AXP2101_FUEL_GAUGE_CTRL, 4);
        } else {
            setRegisterBit(XPOWERS_AXP2101_FUEL_GAUGE_CTRL, 4);
        }
        if (enable) {
            setRegisterBit(XPOWERS_AXP2101_FUEL_GAUGE_CTRL, 0);
        } else {
            clrRegisterBit(XPOWERS_AXP2101_FUEL_GAUGE_CTRL, 0);
        }
    }

uint64_t Axp2101Driver::getIrqStatus(void)
    {
        statusRegister[0] = readRegister(XPOWERS_AXP2101_INTSTS1);
        statusRegister[1] = readRegister(XPOWERS_AXP2101_INTSTS2);
        statusRegister[2] = readRegister(XPOWERS_AXP2101_INTSTS3);
        return (uint32_t)(statusRegister[0] << 16) | (uint32_t)(statusRegister[1] << 8) | (uint32_t)(statusRegister[2]);
    }

void Axp2101Driver::clearIrqStatus()
    {
        for (int i = 0; i < XPOWERS_AXP2101_INTSTS_CNT; i++) {
            writeRegister(XPOWERS_AXP2101_INTSTS1 + i, 0xFF);
            statusRegister[i] = 0;
        }
    }

void Axp2101Driver::printIntRegister()
    {
        for (int i = 0; i < XPOWERS_AXP2101_INTSTS_CNT; i++) {
            uint8_t val =  readRegister(XPOWERS_AXP2101_INTEN1 + i);
            printf("INT[%d] HEX:0x%X\n", i, val);
        }
    }

bool Axp2101Driver::enableIRQ(uint64_t opt)
    {
        return setInterruptImpl(opt, true);
    }

bool Axp2101Driver::disableIRQ(uint64_t opt)
    {
        return setInterruptImpl(opt, false);
    }

bool Axp2101Driver::isDropWarningLevel2Irq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_WARNING_LEVEL2_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isDropWarningLevel1Irq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_WARNING_LEVEL1_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isGaugeWdtTimeoutIrq()
    {
        uint8_t mask = XPOWERS_AXP2101_WDT_TIMEOUT_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatChargerOverTemperatureIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_CHG_OVER_TEMP_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatChargerUnderTemperatureIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_CHG_UNDER_TEMP_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatWorkOverTemperatureIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_NOR_OVER_TEMP_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatWorkUnderTemperatureIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_NOR_UNDER_TEMP_IRQ;
        if (intRegister[0] & mask) {
            return IS_BIT_SET(statusRegister[0], mask);
        }
        return false;
    }

bool Axp2101Driver::isVbusInsertIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_VBUS_INSERT_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isVbusRemoveIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_VBUS_REMOVE_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatInsertIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_INSERT_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatRemoveIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_REMOVE_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isPekeyShortPressIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_PKEY_SHORT_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;

    }

bool Axp2101Driver::isPekeyLongPressIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_PKEY_LONG_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isPekeyNegativeIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isPekeyPositiveIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_PKEY_POSITIVE_IRQ  >> 8;
        if (intRegister[1] & mask) {
            return IS_BIT_SET(statusRegister[1], mask);
        }
        return false;
    }

bool Axp2101Driver::isWdtExpireIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_WDT_EXPIRE_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isLdoOverCurrentIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_LDO_OVER_CURR_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatfetOverCurrentIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BATFET_OVER_CURR_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatChargeDoneIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatChargeStartIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_CHG_START_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatDieOverTemperatureIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_DIE_OVER_TEMP_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isChargeOverTimeoutIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_CHAGER_TIMER_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

bool Axp2101Driver::isBatOverVoltageIrq(void)
    {
        uint8_t mask = XPOWERS_AXP2101_BAT_OVER_VOL_IRQ  >> 16;
        if (intRegister[2] & mask) {
            return IS_BIT_SET(statusRegister[2], mask);
        }
        return false;
    }

uint8_t Axp2101Driver::getChipID(void)
    {
        return readRegister(XPOWERS_AXP2101_IC_TYPE);
    }

uint16_t Axp2101Driver::getPowerChannelVoltage(uint8_t channel)
    {
        switch (channel) {
        case XPOWERS_DCDC1:
            return getDC1Voltage();
        case XPOWERS_DCDC2:
            return getDC2Voltage();
        case XPOWERS_DCDC3:
            return getDC3Voltage();
        case XPOWERS_DCDC4:
            return getDC4Voltage();
        case XPOWERS_DCDC5:
            return getDC5Voltage();
        case XPOWERS_ALDO1:
            return getALDO1Voltage();
        case XPOWERS_ALDO2:
            return getALDO2Voltage();
        case XPOWERS_ALDO3:
            return getALDO3Voltage();
        case XPOWERS_ALDO4:
            return getALDO4Voltage();
        case XPOWERS_BLDO1:
            return getBLDO1Voltage();
        case XPOWERS_BLDO2:
            return getBLDO2Voltage();
        case XPOWERS_DLDO1:
            return getDLDO1Voltage();
        case XPOWERS_DLDO2:
            return getDLDO2Voltage();
        case XPOWERS_VBACKUP:
            return getButtonBatteryVoltage();
        default:
            break;
        }
        return 0;
    }

bool Axp2101Driver::enablePowerOutput(uint8_t channel)
    {
        switch (channel) {
        case XPOWERS_DCDC1:
            return enableDC1();
        case XPOWERS_DCDC2:
            return enableDC2();
        case XPOWERS_DCDC3:
            return enableDC3();
        case XPOWERS_DCDC4:
            return enableDC4();
        case XPOWERS_DCDC5:
            return enableDC5();
        case XPOWERS_ALDO1:
            return enableALDO1();
        case XPOWERS_ALDO2:
            return enableALDO2();
        case XPOWERS_ALDO3:
            return enableALDO3();
        case XPOWERS_ALDO4:
            return enableALDO4();
        case XPOWERS_BLDO1:
            return enableBLDO1();
        case XPOWERS_BLDO2:
            return enableBLDO2();
        case XPOWERS_DLDO1:
            return enableDLDO1();
        case XPOWERS_DLDO2:
            return enableDLDO2();
        case XPOWERS_VBACKUP:
            return enableButtonBatteryCharge();
        default:
            break;
        }
        return false;
    }

bool Axp2101Driver::disablePowerOutput(uint8_t channel)
    {
        if (getProtectedChannel(channel)) {
            log_e("Failed to disable the power channel, the power channel has been protected");
            return false;
        }
        switch (channel) {
        case XPOWERS_DCDC1:
            return disableDC1();
        case XPOWERS_DCDC2:
            return disableDC2();
        case XPOWERS_DCDC3:
            return disableDC3();
        case XPOWERS_DCDC4:
            return disableDC4();
        case XPOWERS_DCDC5:
            return disableDC5();
        case XPOWERS_ALDO1:
            return disableALDO1();
        case XPOWERS_ALDO2:
            return disableALDO2();
        case XPOWERS_ALDO3:
            return disableALDO3();
        case XPOWERS_ALDO4:
            return disableALDO4();
        case XPOWERS_BLDO1:
            return disableBLDO1();
        case XPOWERS_BLDO2:
            return disableBLDO2();
        case XPOWERS_DLDO1:
            return disableDLDO1();
        case XPOWERS_DLDO2:
            return disableDLDO2();
        case XPOWERS_VBACKUP:
            return disableButtonBatteryCharge();
        case XPOWERS_CPULDO:
            return disableCPUSLDO();
        default:
            break;
        }
        return false;
    }

bool Axp2101Driver::isPowerChannelEnable(uint8_t channel)
    {
        switch (channel) {
        case XPOWERS_DCDC1:
            return isEnableDC1();
        case XPOWERS_DCDC2:
            return isEnableDC2();
        case XPOWERS_DCDC3:
            return isEnableDC3();
        case XPOWERS_DCDC4:
            return isEnableDC4();
        case XPOWERS_DCDC5:
            return isEnableDC5();
        case XPOWERS_ALDO1:
            return isEnableALDO1();
        case XPOWERS_ALDO2:
            return isEnableALDO2();
        case XPOWERS_ALDO3:
            return isEnableALDO3();
        case XPOWERS_ALDO4:
            return isEnableALDO4();
        case XPOWERS_BLDO1:
            return isEnableBLDO1();
        case XPOWERS_BLDO2:
            return isEnableBLDO2();
        case XPOWERS_DLDO1:
            return isEnableDLDO1();
        case XPOWERS_DLDO2:
            return isEnableDLDO2();
        case XPOWERS_VBACKUP:
            return isEnableButtonBatteryCharge();
        case XPOWERS_CPULDO:
            return isEnableCPUSLDO();
        default:
            break;
        }
        return false;
    }

bool Axp2101Driver::setPowerChannelVoltage(uint8_t channel, uint16_t millivolt)
    {
        if (getProtectedChannel(channel)) {
            log_e("Failed to set the power channel, the power channel has been protected");
            return false;
        }
        switch (channel) {
        case XPOWERS_DCDC1:
            return setDC1Voltage(millivolt);
        case XPOWERS_DCDC2:
            return setDC2Voltage(millivolt);
        case XPOWERS_DCDC3:
            return setDC3Voltage(millivolt);
        case XPOWERS_DCDC4:
            return setDC4Voltage(millivolt);
        case XPOWERS_DCDC5:
            return setDC5Voltage(millivolt);
        case XPOWERS_ALDO1:
            return setALDO1Voltage(millivolt);
        case XPOWERS_ALDO2:
            return setALDO2Voltage(millivolt);
        case XPOWERS_ALDO3:
            return setALDO3Voltage(millivolt);
        case XPOWERS_ALDO4:
            return setALDO4Voltage(millivolt);
        case XPOWERS_BLDO1:
            return setBLDO1Voltage(millivolt);
        case XPOWERS_BLDO2:
            return setBLDO1Voltage(millivolt);
        case XPOWERS_DLDO1:
            return setDLDO1Voltage(millivolt);
        case XPOWERS_DLDO2:
            return setDLDO1Voltage(millivolt);
        case XPOWERS_VBACKUP:
            return setButtonBatteryChargeVoltage(millivolt);
        case XPOWERS_CPULDO:
            return setCPUSLDOVoltage(millivolt);
        default:
            break;
        }
        return false;
    }

bool Axp2101Driver::initImpl()
    {
        int chip_id = readRegister(XPOWERS_AXP2101_IC_TYPE);
        if (chip_id < 0) {
            log_e("Unable to read AXP2101 chip ID register 0x%02X", XPOWERS_AXP2101_IC_TYPE);
            return false;
        }
        if (chip_id == XPOWERS_AXP2101_CHIP_ID) {
            setChipModel(XPOWERS_AXP2101);
            disableTSPinMeasure();      //Disable NTC temperature detection by default
            return true;
        }
        log_e("Unexpected AXP chip ID 0x%02X, expected 0x%02X", chip_id,
              XPOWERS_AXP2101_CHIP_ID);
        return  false;
    }

bool Axp2101Driver::setInterruptImpl(uint32_t opts, bool enable)
    {
        int res = 0;
        uint8_t data = 0, value = 0;
        log_d("%s - HEX:0x%x \n", enable ? "ENABLE" : "DISABLE", opts);
        if (opts & 0x0000FF) {
            value = opts & 0xFF;
            // log_d("Write INT0: %x\n", value);
            data = readRegister(XPOWERS_AXP2101_INTEN1);
            intRegister[0] =  enable ? (data | value) : (data & (~value));
            res |= writeRegister(XPOWERS_AXP2101_INTEN1, intRegister[0]);
        }
        if (opts & 0x00FF00) {
            value = opts >> 8;
            // log_d("Write INT1: %x\n", value);
            data = readRegister(XPOWERS_AXP2101_INTEN2);
            intRegister[1] =  enable ? (data | value) : (data & (~value));
            res |= writeRegister(XPOWERS_AXP2101_INTEN2, intRegister[1]);
        }
        if (opts & 0xFF0000) {
            value = opts >> 16;
            // log_d("Write INT2: %x\n", value);
            data = readRegister(XPOWERS_AXP2101_INTEN3);
            intRegister[2] =  enable ? (data | value) : (data & (~value));
            res |= writeRegister(XPOWERS_AXP2101_INTEN3, intRegister[2]);
        }
        return res == 0;
    }

const char  *Axp2101Driver::getChipNameImpl(void)
    {
        return "AXP2101";
    }
