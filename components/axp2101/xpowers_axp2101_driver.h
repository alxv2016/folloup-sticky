#ifndef AXP2101_DRIVER_H
#define AXP2101_DRIVER_H

#include <esp_log.h>

#include "xpowers_axp2101_common.h"
#include "xpowers_axp2101_constants.h"
#include "xpowers_axp2101_params.h"

#define AXP2101_DRIVER_LOG_TAG "Axp2101Driver"
#define log_e(fmt, ...) ESP_LOGE(AXP2101_DRIVER_LOG_TAG, fmt, ##__VA_ARGS__)
#define log_i(fmt, ...) ESP_LOGI(AXP2101_DRIVER_LOG_TAG, fmt, ##__VA_ARGS__)
#define log_d(fmt, ...) ESP_LOGD(AXP2101_DRIVER_LOG_TAG, fmt, ##__VA_ARGS__)

#ifndef IS_BIT_SET
#define IS_BIT_SET(val, mask) (((val) & (mask)) == (mask))
#endif

#ifndef constrain
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

class Axp2101Driver : public Axp2101Common {
public:
    Axp2101Driver(i2c_master_bus_handle_t i2c_bus, uint8_t addr, uint32_t scl_speed_hz = 400 * 1000);
    ~Axp2101Driver();

    bool init();

    void deinit();

    /*
     * PMU status functions
     */
    uint16_t status();

    bool isVbusGood(void);

    bool getBatfetState(void);

    // getBatPresentState
    bool isBatteryConnect(void);

    bool isBatInActiveModeState(void);

    bool getThermalRegulationStatus(void);

    bool getCurrentLimitStatus(void);

    bool isCharging(void);

    bool isDischarge(void);

    bool isStandby(void);

    bool isPowerOn(void);

    bool isPowerOff(void);

    bool isVbusIn(void);

    xpowers_chg_status_t getChargerStatus(void);

    /*
     * Data Buffer
     */

    bool writeDataBuffer(uint8_t *data, uint8_t size);

    bool readDataBuffer(uint8_t *data, uint8_t size);

    /*
     * PMU common configuration
     */

    /**
     * @brief   Internal off-discharge enable for DCDC & LDO & SWITCH
     */

    void enableInternalDischarge(void);

    void disableInternalDischarge(void);


    /**
     * @brief   PWROK PIN pull low to Restart
     */
    void enablePwrOkPinPullLow(void);

    void disablePwrOkPinPullLow(void);

    void enablePwronShutPMIC(void);

    void disablePwronShutPMIC(void);


    /**
     * @brief  Restart the SoC System, POWOFF/POWON and reset the related registers
     * @retval None
     */
    void reset(void);

    /**
     * @brief  Set shutdown, calling shutdown will turn off all power channels,
     *         only VRTC belongs to normal power supply
     * @retval None
     */
    void shutdown(void);

    /**
     * @brief  BATFET control / REG 12H
     * @note   DIE Over Temperature Protection Level1 Configuration
     * @param  opt: 0:115 , 1:125 , 2:135
     * @retval None
     */
    void setBatfetDieOverTempLevel1(uint8_t opt);

    uint8_t getBatfetDieOverTempLevel1(void);

    void enableBatfetDieOverTempDetect(void);

    void disableBatfetDieOverTempDetect(void);

    /**
     * @param  opt: 0:115 , 1:125 , 2:135
     */
    void setDieOverTempLevel1(uint8_t opt);

    uint8_t getDieOverTempLevel1(void);

    void enableDieOverTempDetect(void);

    void disableDieOverTempDetect(void);

    // Linear Charger Vsys voltage dpm
    void setLinearChargerVsysDpm(xpower_chg_dpm_t opt);

    uint8_t getLinearChargerVsysDpm(void);


    /**
     * @brief  Set VBUS Voltage Input Limit.
     * @param  opt: View the related chip type xpowers_axp2101_vbus_vol_limit_t enumeration
     *              parameters in "XPowersParams.hpp"
     */
    void setVbusVoltageLimit(uint8_t opt);

    /**
    * @brief  Get VBUS Voltage Input Limit.
    * @retval View the related chip type xpowers_axp2101_vbus_vol_limit_t enumeration
    *              parameters in "XPowersParams.hpp"
    */
    uint8_t getVbusVoltageLimit(void);

    /**
    * @brief  Set VBUS Current Input Limit.
    * @param  opt: View the related chip type xpowers_axp2101_vbus_cur_limit_t enumeration
    *              parameters in "XPowersParams.hpp"
    * @retval true valid false invalid
    */
    bool setVbusCurrentLimit(uint8_t opt);

    /**
    * @brief  Get VBUS Current Input Limit.
    * @retval View the related chip type xpowers_axp2101_vbus_cur_limit_t enumeration
    *              parameters in "XPowersParams.hpp"
    */
    uint8_t getVbusCurrentLimit(void);

    /**
     * @brief  Reset the fuel gauge
     */
    void resetGauge(void);

    /**
     * @brief   reset the gauge besides reset
     */
    void resetGaugeBesides(void);

    /**
     * @brief Gauge Module
     */
    void enableGauge(void);

    void disableGauge(void);

    /**
     * @brief  Button Battery charge
     */
    bool enableButtonBatteryCharge(void);

    bool disableButtonBatteryCharge(void);

    bool isEnableButtonBatteryCharge();


    //Button battery charge termination voltage setting
    bool setButtonBatteryChargeVoltage(uint16_t millivolt);

    uint16_t getButtonBatteryVoltage(void);


    /**
     * @brief Cell Battery charge
     */
    void enableCellbatteryCharge(void);

    void disableCellbatteryCharge(void);

    /**
     * @brief  Watchdog Module
     */
    void enableWatchdog(void);

    void disableWatchdog(void);

    /**
     * @brief Watchdog Config
     * @note
     * @param  opt: 0: IRQ Only 1: IRQ and System reset  2: IRQ, System Reset and Pull down PWROK 1s  3: IRQ, System Reset, DCDC/LDO PWROFF & PWRON
     * @retval None
     */
    void setWatchdogConfig(xpowers_wdt_config_t opt);

    uint8_t getWatchConfig(void);

    void clrWatchdog(void);


    void setWatchdogTimeout(xpowers_wdt_timeout_t opt);

    uint8_t getWatchdogTimerout(void);

    /**
     * @brief  Low battery warning threshold 5-20%, 1% per step
     * @param  percentage:   5 ~ 20
     * @retval None
     */
    void setLowBatWarnThreshold(uint8_t percentage);

    uint8_t getLowBatWarnThreshold(void);

    /**
     * @brief  Low battery shutdown threshold 0-15%, 1% per step
     * @param  opt:   0 ~ 15
     * @retval None
     */
    void setLowBatShutdownThreshold(uint8_t opt);

    uint8_t getLowBatShutdownThreshold(void);

    //!  PWRON statu  20
    // POWERON always high when EN Mode as POWERON Source
    bool isPoweronAlwaysHighSource();

    // Battery Insert and Good as POWERON Source
    bool isBattInsertOnSource();

    // Battery Voltage > 3.3V when Charged as Source
    bool isBattNormalOnSource();

    // Vbus Insert and Good as POWERON Source
    bool isVbusInsertOnSource();

    // IRQ PIN Pull-down as POWERON Source
    bool isIrqLowOnSource();

    // POWERON low for on level when POWERON Mode as POWERON Source
    bool isPwronLowOnSource();

    xpower_power_on_source_t getPowerOnSource();

    //!  PWROFF status  21
    // Die Over Temperature as POWEROFF Source
    bool isOverTemperatureOffSource();

    // DCDC Over Voltage as POWEROFF Source
    bool isDcOverVoltageOffSource();

    // DCDC Under Voltage as POWEROFF Source
    bool isDcUnderVoltageOffSource();

    // VBUS Over Voltage as POWEROFF Source
    bool isVbusOverVoltageOffSource();

    // Vsys Under Voltage as POWEROFF Source
    bool isVsysUnderVoltageOffSource();

    // POWERON always low when EN Mode as POWEROFF Source
    bool isPwronAlwaysLowOffSource();

    // Software configuration as POWEROFF Source
    bool isSwConfigOffSource();

    // POWERON Pull down for off level when POWERON Mode as POWEROFF Source
    bool isPwrSourcePullDown();

    xpower_power_off_source_t getPowerOffSource();

    //!REG 22H
    void enableOverTemperatureLevel2PowerOff();

    void disableOverTemperaturePowerOff();

    // CHANGE:  void enablePwrOnOverVolOffLevelPowerOff()
    void enableLongPressShutdown();

    // CHANGE:  void disablePwrOnOverVolOffLevelPowerOff()
    void disableLongPressShutdown();

    //CHANGE: void enablePwrOffSelectFunction()
    void setLongPressRestart();

    //CHANGE: void disablePwrOffSelectFunction()
    void setLongPressPowerOFF();

    //!REG 23H
    // DCDC 120%(130%) high voltage turn off PMIC function
    void enableDCHighVoltageTurnOff();

    void disableDCHighVoltageTurnOff();

    // DCDC5 85% low voltage turn Off PMIC function
    void enableDC5LowVoltageTurnOff();

    void disableDC5LowVoltageTurnOff();

    // DCDC4 85% low voltage turn Off PMIC function
    void enableDC4LowVoltageTurnOff();

    void disableDC4LowVoltageTurnOff();

    // DCDC3 85% low voltage turn Off PMIC function
    void enableDC3LowVoltageTurnOff();

    void disableDC3LowVoltageTurnOff();

    // DCDC2 85% low voltage turn Off PMIC function
    void enableDC2LowVoltageTurnOff();

    void disableDC2LowVoltageTurnOff();

    // DCDC1 85% low voltage turn Off PMIC function
    void enableDC1LowVoltageTurnOff();

    void disableDC1LowVoltageTurnOff();


    // Set the minimum system operating voltage inside the PMU,
    // below this value will shut down the PMU,Adjustment range 2600mV~3300mV
    bool setSysPowerDownVoltage(uint16_t millivolt);

    uint16_t getSysPowerDownVoltage(void);

    //  PWROK setting and PWROFF sequence control 25.
    // Check the PWROK Pin enable after all dcdc/ldo output valid 128ms
    void enablePwrOk();

    void disablePwrOk();

    // POWEROFF Delay 4ms after PWROK enable
    void enablePowerOffDelay();

    // POWEROFF Delay 4ms after PWROK disable
    void disablePowerOffDelay();

    // POWEROFF Sequence Control the reverse of the Startup
    void enablePowerSequence();

    // POWEROFF Sequence Control at the same time
    void disablePowerSequence();

    // Delay of PWROK after all power output good
    bool setPwrOkDelay(xpower_pwrok_delay_t opt);

    xpower_pwrok_delay_t getPwrOkDelay();

    //  Sleep and 26
    void wakeupControl(xpowers_wakeup_t opt, bool enable);

    bool enableWakeup(void);

    bool disableWakeup(void);

    bool enableSleep(void);

    bool disableSleep(void);


    //  RQLEVEL/OFFLEVEL/ONLEVEL setting 27
    /**
     * @brief  IRQLEVEL configur
     * @param  opt: 0:1s  1:1.5s  2:2s 3:2.5s
     */
    void setIrqLevel(uint8_t opt);

    /**
     * @brief  OFFLEVEL configuration
     * @param  opt:  0:4s 1:6s 2:8s 3:10s
     */
    void setOffLevel(uint8_t opt);

    /**
     * @brief  ONLEVEL configuration
     * @param  opt: 0:128ms 1:512ms 2:1s  3:2s
     */
    void setOnLevel(uint8_t opt);

    // Fast pwron setting 0  28
    // Fast Power On Start Sequence
    void setDc4FastStartSequence(xpower_start_sequence_t opt);

    void setDc3FastStartSequence(xpower_start_sequence_t opt);
    void setDc2FastStartSequence(xpower_start_sequence_t opt);
    void setDc1FastStartSequence(xpower_start_sequence_t opt);

    //  Fast pwron setting 1  29
    void setAldo3FastStartSequence(xpower_start_sequence_t opt);
    void setAldo2FastStartSequence(xpower_start_sequence_t opt);
    void setAldo1FastStartSequence(xpower_start_sequence_t opt);

    void setDc5FastStartSequence(xpower_start_sequence_t opt);

    //  Fast pwron setting 2  2A
    void setCpuldoFastStartSequence(xpower_start_sequence_t opt);

    void setBldo2FastStartSequence(xpower_start_sequence_t opt);

    void setBldo1FastStartSequence(xpower_start_sequence_t opt);

    void setAldo4FastStartSequence(xpower_start_sequence_t opt);

    //  Fast pwron setting 3  2B
    void setDldo2FastStartSequence(xpower_start_sequence_t opt);

    void setDldo1FastStartSequence(xpower_start_sequence_t opt);

    /**
     * @brief   Setting Fast Power On Start Sequence
     */
    void setFastPowerOnLevel(xpowers_fast_on_opt_t opt, xpower_start_sequence_t seq_level);

    void disableFastPowerOn(xpowers_fast_on_opt_t opt);

    void enableFastPowerOn(void);

    void disableFastPowerOn(void);

    void enableFastWakeup(void);

    void disableFastWakeup(void);

    // DCDC 120%(130%) high voltage turn off PMIC function
    void setDCHighVoltagePowerDown(bool en);

    bool getDCHighVoltagePowerDownEn();

    // DCDCS force PWM control
    void setDcUVPDebounceTime(uint8_t opt);

    void settDC1WorkModeToPwm(uint8_t enable);

    void settDC2WorkModeToPwm(uint8_t enable);

    void settDC3WorkModeToPwm(uint8_t enable);

    void settDC4WorkModeToPwm( uint8_t enable);

    //1 = 100khz 0=50khz
    void setDCFreqSpreadRange(uint8_t opt);

    void setDCFreqSpreadRangeEn(bool en);

    void enableCCM();

    void disableCCM();

    bool isenableCCM();

    enum DVMRamp {
        XPOWERS_AXP2101_DVM_RAMP_15_625US,
        XPOWERS_AXP2101_DVM_RAMP_31_250US,
    };

    //args:enum DVMRamp
    void setDVMRamp(uint8_t opt);



    /*
     * Power control DCDC1 functions
     */
    bool isEnableDC1(void);

    bool enableDC1(void);

    bool disableDC1(void);

    bool setDC1Voltage(uint16_t millivolt);

    uint16_t getDC1Voltage(void);



    // DCDC1 85% low voltage turn off PMIC function
    void setDC1LowVoltagePowerDown(bool en);

    bool getDC1LowVoltagePowerDownEn();

    /*
     * Power control DCDC2 functions
     */
    bool isEnableDC2(void);

    bool enableDC2(void);

    bool disableDC2(void);

    bool setDC2Voltage(uint16_t millivolt);

    uint16_t getDC2Voltage(void);

    uint8_t getDC2WorkMode(void);

    void setDC2LowVoltagePowerDown(bool en);

    bool getDC2LowVoltagePowerDownEn();

    /*
     * Power control DCDC3 functions
     */

    bool isEnableDC3(void);

    bool enableDC3(void);

    bool disableDC3(void);

    /**
        0.5~1.2V,10mV/step,71steps
        1.22~1.54V,20mV/step,17steps
        1.6~3.4V,100mV/step,19steps
     */
    bool setDC3Voltage(uint16_t millivolt);


    uint16_t getDC3Voltage(void);

    uint8_t getDC3WorkMode(void);

    // DCDC3 85% low voltage turn off PMIC function
    void setDC3LowVoltagePowerDown(bool en);

    bool getDC3LowVoltagePowerDownEn();


    /*
    * Power control DCDC4 functions
    */
    /**
        0.5~1.2V,10mV/step,71steps
        1.22~1.84V,20mV/step,32steps
     */
    bool isEnableDC4(void);

    bool enableDC4(void);

    bool disableDC4(void);

    bool setDC4Voltage(uint16_t millivolt);

    uint16_t getDC4Voltage(void);

    // DCDC4 85% low voltage turn off PMIC function
    void setDC4LowVoltagePowerDown(bool en);

    bool getDC4LowVoltagePowerDownEn();

    /*
    * Power control DCDC5 functions,Output to gpio pin
    */
    bool isEnableDC5(void);

    bool enableDC5(void);

    bool disableDC5(void);

    bool setDC5Voltage(uint16_t millivolt);

    uint16_t getDC5Voltage(void);

    bool isDC5FreqCompensationEn(void);

    void enableDC5FreqCompensation();

    void disableFreqCompensation();

    // DCDC4 85% low voltage turn off PMIC function
    void setDC5LowVoltagePowerDown(bool en);

    bool getDC5LowVoltagePowerDownEn();

    /*
    * Power control ALDO1 functions
    */
    bool isEnableALDO1(void);

    bool enableALDO1(void);

    bool disableALDO1(void);

    bool setALDO1Voltage(uint16_t millivolt);

    uint16_t getALDO1Voltage(void);

    /*
    * Power control ALDO2 functions
    */
    bool isEnableALDO2(void);

    bool enableALDO2(void);

    bool disableALDO2(void);

    bool setALDO2Voltage(uint16_t millivolt);

    uint16_t getALDO2Voltage(void);

    /*
     * Power control ALDO3 functions
     */
    bool isEnableALDO3(void);

    bool enableALDO3(void);

    bool disableALDO3(void);

    bool setALDO3Voltage(uint16_t millivolt);

    uint16_t getALDO3Voltage(void);

    /*
     * Power control ALDO4 functions
     */
    bool isEnableALDO4(void);

    bool enableALDO4(void);

    bool disableALDO4(void);

    bool setALDO4Voltage(uint16_t millivolt);

    uint16_t getALDO4Voltage(void);

    /*
    * Power control BLDO1 functions
    */
    bool isEnableBLDO1(void);

    bool enableBLDO1(void);

    bool disableBLDO1(void);

    bool setBLDO1Voltage(uint16_t millivolt);

    uint16_t getBLDO1Voltage(void);

    /*
    * Power control BLDO2 functions
    */
    bool isEnableBLDO2(void);

    bool enableBLDO2(void);

    bool disableBLDO2(void);

    bool setBLDO2Voltage(uint16_t millivolt);

    uint16_t getBLDO2Voltage(void);

    /*
    * Power control CPUSLDO functions
    */
    bool isEnableCPUSLDO(void);

    bool enableCPUSLDO(void);

    bool disableCPUSLDO(void);

    bool setCPUSLDOVoltage(uint16_t millivolt);

    uint16_t getCPUSLDOVoltage(void);


    /*
    * Power control DLDO1 functions
    */
    bool isEnableDLDO1(void);

    bool enableDLDO1(void);

    bool disableDLDO1(void);

    bool setDLDO1Voltage(uint16_t millivolt);

    uint16_t getDLDO1Voltage(void);

    /*
    * Power control DLDO2 functions
    */
    bool isEnableDLDO2(void);

    bool enableDLDO2(void);

    bool disableDLDO2(void);

    bool setDLDO2Voltage(uint16_t millivolt);

    uint16_t getDLDO2Voltage(void);


    /*
     * Power ON OFF IRQ TIMMING Control method
     */

    void setIrqLevelTime(xpowers_irq_time_t opt);

    xpowers_irq_time_t getIrqLevelTime(void);

    /**
    * @brief Set the PEKEY power-on long press time.
    * @param opt: See xpowers_press_on_time_t enum for details.
    * @retval
    */
    bool setPowerKeyPressOnTime(uint8_t opt);

    /**
    * @brief Get the PEKEY power-on long press time.
    * @retval See xpowers_press_on_time_t enum for details.
    */
    uint8_t getPowerKeyPressOnTime(void);

    /**
    * @brief Set the PEKEY power-off long press time.
    * @param opt: See xpowers_press_off_time_t enum for details.
    * @retval
    */
    bool setPowerKeyPressOffTime(uint8_t opt);

    /**
    * @brief Get the PEKEY power-off long press time.
    * @retval See xpowers_press_off_time_t enum for details.
    */
    uint8_t getPowerKeyPressOffTime(void);

    /*
     * ADC Control method
     */
    bool enableGeneralAdcChannel(void);

    bool disableGeneralAdcChannel(void);

    bool enableTemperatureMeasure(void);

    bool disableTemperatureMeasure(void);

    float getTemperature(void);

    bool enableSystemVoltageMeasure(void);

    bool disableSystemVoltageMeasure(void);

    uint16_t getSystemVoltage(void);

    bool enableVbusVoltageMeasure(void);

    bool disableVbusVoltageMeasure(void);

    uint16_t getVbusVoltage(void);

    bool enableTSPinMeasure(void);

    bool disableTSPinMeasure(void);

    bool enableTSPinLowFreqSample(void);

    bool disableTSPinLowFreqSample(void);

    uint16_t getTsTemperature(void);

    bool enableBattVoltageMeasure(void);

    bool disableBattVoltageMeasure(void);

    bool enableBattDetection(void);

    bool disableBattDetection(void);

    uint16_t getBattVoltage(void);

    int getBatteryPercent(void);

    /*
    * CHG LED setting and control
    */
    // void enableChargingLed(void)
    // {
    //     setRegisterBit(XPOWERS_AXP2101_CHGLED_SET_CTRL, 0);
    // }

    // void disableChargingLed(void)
    // {
    //     clrRegisterBit(XPOWERS_AXP2101_CHGLED_SET_CTRL, 0);
    // }

    /**
    * @brief Set charging led mode.
    * @retval See xpowers_chg_led_mode_t enum for details.
    */
    void setChargingLedMode(uint8_t mode);

    uint8_t getChargingLedMode();

    /**
     * @brief 预充电充电电流限制
     * @note  Precharge current limit 25*N mA
     * @param  opt: 25 * opt
     * @retval None
     */
    void setPrechargeCurr(xpowers_prechg_t opt);

    xpowers_prechg_t getPrechargeCurr(void);


    /**
    * @brief Set charge current.
    * @param  opt: See xpowers_axp2101_chg_curr_t enum for details.
    * @retval
    */
    bool setChargerConstantCurr(uint8_t opt);

    /**
     * @brief Get charge current settings.
    *  @retval See xpowers_axp2101_chg_curr_t enum for details.
     */
    uint8_t getChargerConstantCurr(void);

    /**
     * @brief  充电终止电流限制
     * @note   Charging termination of current limit
     * @retval
     */
    void setChargerTerminationCurr(xpowers_axp2101_chg_iterm_t opt);

    xpowers_axp2101_chg_iterm_t getChargerTerminationCurr(void);

    void enableChargerTerminationLimit(void);

    void disableChargerTerminationLimit(void);

    bool isChargerTerminationLimit(void);


    /**
    * @brief Set charge target voltage.
    * @param  opt: See xpowers_axp2101_chg_vol_t enum for details.
    * @retval
    */
    bool setChargeTargetVoltage(uint8_t opt);

    /**
     * @brief Get charge target voltage settings.
     * @retval See xpowers_axp2101_chg_vol_t enum for details.
     */
    uint8_t getChargeTargetVoltage(void);


    /**
     * @brief  设定热阈值
     * @note   Thermal regulation threshold setting
     */
    void setThermaThreshold(xpowers_thermal_t opt);

    xpowers_thermal_t getThermaThreshold(void);

    uint8_t getBatteryParameter();

    void fuelGaugeControl(bool writeROM, bool enable);

    /*
     * Interrupt status/control functions
     */

    /**
    * @brief  Get the interrupt controller mask value.
    * @retval   Mask value corresponds to xpowers_axp2101_irq_t ,
    */
    uint64_t getIrqStatus(void);


    /**
     * @brief  Clear interrupt controller state.
     */
    void clearIrqStatus();

    /*
    *  @brief  Debug interrupt setting register
    * */
    void printIntRegister();

    /**
     * @brief  Enable PMU interrupt control mask .
     * @param  opt: View the related chip type xpowers_axp2101_irq_t enumeration
     *              parameters in "XPowersParams.hpp"
     * @retval
     */
    bool enableIRQ(uint64_t opt);

    /**
     * @brief  Disable PMU interrupt control mask .
     * @param  opt: View the related chip type xpowers_axp2101_irq_t enumeration
     *              parameters in "XPowersParams.hpp"
     * @retval
     */
    bool disableIRQ(uint64_t opt);

    //IRQ STATUS 0
    bool isDropWarningLevel2Irq(void);

    bool isDropWarningLevel1Irq(void);

    bool isGaugeWdtTimeoutIrq();

    bool isBatChargerOverTemperatureIrq(void);

    bool isBatChargerUnderTemperatureIrq(void);

    bool isBatWorkOverTemperatureIrq(void);

    bool isBatWorkUnderTemperatureIrq(void);

    //IRQ STATUS 1
    bool isVbusInsertIrq(void);

    bool isVbusRemoveIrq(void);

    bool isBatInsertIrq(void);

    bool isBatRemoveIrq(void);

    bool isPekeyShortPressIrq(void);

    bool isPekeyLongPressIrq(void);

    bool isPekeyNegativeIrq(void);

    bool isPekeyPositiveIrq(void);

    //IRQ STATUS 2
    bool isWdtExpireIrq(void);

    bool isLdoOverCurrentIrq(void);

    bool isBatfetOverCurrentIrq(void);

    bool isBatChargeDoneIrq(void);

    bool isBatChargeStartIrq(void);

    bool isBatDieOverTemperatureIrq(void);

    bool isChargeOverTimeoutIrq(void);

    bool isBatOverVoltageIrq(void);


    uint8_t getChipID(void);

protected:

    uint16_t getPowerChannelVoltage(uint8_t channel);

    bool enablePowerOutput(uint8_t channel);

    bool disablePowerOutput(uint8_t channel);

    bool isPowerChannelEnable(uint8_t channel);

    bool setPowerChannelVoltage(uint8_t channel, uint16_t millivolt);

    bool initImpl();

    /*
     * Interrupt control functions
     */
    bool setInterruptImpl(uint32_t opts, bool enable);

    const char  *getChipNameImpl(void);

protected:
    void setProtectedChannel(uint8_t channel);
    void clearProtectedChannel(uint8_t channel);
    bool getProtectedChannel(uint8_t channel) const;

    uint8_t getChipModel() const;
    void setChipModel(uint8_t model);

private:
    uint32_t protected_channel_mask_ = 0;
    uint8_t chip_model_ = XPOWERS_UNDEFINED;

    uint8_t statusRegister[XPOWERS_AXP2101_INTSTS_CNT];
    uint8_t intRegister[XPOWERS_AXP2101_INTSTS_CNT];
};

#endif  // AXP2101_DRIVER_H
