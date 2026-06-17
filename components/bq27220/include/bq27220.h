#pragma once

#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef union {
    struct {
        bool DSG : 1;
        bool SYSDWN : 1;
        bool TDA : 1;
        bool BATTPRES : 1;
        bool AUTH_GD : 1;
        bool OCVGD : 1;
        bool TCA : 1;
        bool RSVD0 : 1;
        bool CHGINH : 1;
        bool FC : 1;
        bool OTD : 1;
        bool OTC : 1;
        bool SLEEP : 1;
        bool OCVFAIL : 1;
        bool OCVCOMP : 1;
        bool FD : 1;
    };
    uint16_t full;
} battery_status_t;

static_assert(sizeof(battery_status_t) == 2, "Incorrect battery_status_t size");

typedef struct {
    bool CALMD : 1;
    uint8_t SEC : 2;
    bool EDV2 : 1;
    bool VDQ : 1;
    bool INITCOMP : 1;
    bool SMTH : 1;
    bool BTPINT : 1;
    uint8_t RSVD0 : 2;
    bool CFGUPDATE : 1;
    uint8_t RSVD1 : 5;
} operation_status_t;

static_assert(sizeof(operation_status_t) == 2, "Incorrect operation_status_t size");

typedef union {
    struct {
        bool CCT : 1;
        bool CSYNC : 1;
        bool RSVD0 : 1;
        bool EDV_CMP : 1;
        bool SC : 1;
        bool FIXED_EDV0 : 1;
        uint8_t RSVD1 : 2;
        bool FCC_LIM : 1;
        bool RSVD2 : 1;
        bool FC_FOR_VDQ : 1;
        bool IGNORE_SD : 1;
        bool SME0 : 1;
        uint8_t RSVD3 : 3;
    };
    uint16_t full;
} gauging_config_t;

static_assert(sizeof(gauging_config_t) == 2, "Incorrect gauging_config_t size");

typedef struct {
    uint16_t full_charge_cap;
    uint16_t design_cap;
    uint16_t reserve_cap;
    uint16_t near_full;
    uint16_t self_discharge_rate;
    uint16_t EDV0;
    uint16_t EDV1;
    uint16_t EDV2;
    uint16_t EMF;
    uint16_t C0;
    uint16_t R0;
    uint16_t T0;
    uint16_t R1;
    uint8_t TC;
    uint8_t C1;

    uint16_t DOD0;
    uint16_t DOD10;
    uint16_t DOD20;
    uint16_t DOD30;
    uint16_t DOD40;
    uint16_t DOD50;
    uint16_t DOD60;
    uint16_t DOD70;
    uint16_t DOD80;
    uint16_t DOD90;
    uint16_t DOD100;
} parameter_cedv_t;

typedef struct {
    i2c_master_dev_handle_t i2c_dev = nullptr;
    // bq27220_create() expects profile data to be provided.
    const gauging_config_t* cfg = nullptr;
    const parameter_cedv_t* cedv = nullptr;
} bq27220_config_t;

typedef void* bq27220_handle_t;

bq27220_handle_t bq27220_create(const bq27220_config_t* config);
esp_err_t bq27220_delete(bq27220_handle_t bq_handle);

uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle);
uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle);

esp_err_t bq27220_seal(bq27220_handle_t bq_handle);
esp_err_t bq27220_unseal(bq27220_handle_t bq_handle);

esp_err_t bq27220_set_parameter_u16(bq27220_handle_t bq_handle, uint16_t address, uint16_t value);
uint16_t bq27220_get_parameter_u16(bq27220_handle_t bq_handle, uint16_t address);

uint16_t bq27220_get_voltage(bq27220_handle_t bq_handle);
int16_t bq27220_get_current(bq27220_handle_t bq_handle);
int16_t bq27220_get_avgcurrent(bq27220_handle_t bq_handle);
uint16_t bq27220_get_cycle_count(bq27220_handle_t bq_handle);
esp_err_t bq27220_get_battery_status(bq27220_handle_t bq_handle, battery_status_t* battery_status);
esp_err_t bq27220_get_operation_status(bq27220_handle_t bq_handle, operation_status_t* operation_status);
uint16_t bq27220_get_temperature(bq27220_handle_t bq_handle);
uint16_t bq27220_get_full_charge_capacity(bq27220_handle_t bq_handle);
uint16_t bq27220_get_design_capacity(bq27220_handle_t bq_handle);
uint16_t bq27220_get_remaining_capacity(bq27220_handle_t bq_handle);
uint16_t bq27220_get_state_of_charge(bq27220_handle_t bq_handle);
uint16_t bq27220_get_state_of_health(bq27220_handle_t bq_handle);
uint16_t bq27220_get_charge_voltage(bq27220_handle_t bq_handle);
uint16_t bq27220_get_charge_current(bq27220_handle_t bq_handle);
int16_t bq27220_get_average_power(bq27220_handle_t bq_handle);
uint16_t bq27220_get_time_to_empty(bq27220_handle_t bq_handle);
uint16_t bq27220_get_time_to_full(bq27220_handle_t bq_handle);
int16_t bq27220_get_maxload_current(bq27220_handle_t bq_handle);
int16_t bq27220_get_standby_current(bq27220_handle_t bq_handle);

struct Bq27220Data_t {
    uint16_t voltage_mv = 0;
    int16_t current_ma = 0;
    int16_t average_current_ma = 0;
    uint16_t remaining_capacity_mah = 0;
    uint16_t full_charge_capacity_mah = 0;
    uint16_t state_of_charge_percent = 0;
    uint16_t state_of_health_percent = 0;
    uint16_t cycle_count = 0;
    float temperature_c = 0.0f;
    uint16_t battery_status = 0;
};

namespace Bq27220Reg {
constexpr uint8_t Control = 0x00;
constexpr uint8_t Temperature = 0x06;
constexpr uint8_t Voltage = 0x08;
constexpr uint8_t BatteryStatus = 0x0A;
constexpr uint8_t Current = 0x0C;
constexpr uint8_t RemainingCapacity = 0x10;
constexpr uint8_t FullChargeCapacity = 0x12;
constexpr uint8_t AverageCurrent = 0x14;
constexpr uint8_t CycleCount = 0x2A;
constexpr uint8_t StateOfCharge = 0x2C;
constexpr uint8_t StateOfHealth = 0x2E;
constexpr uint8_t BtpDischargeSet = 0x34;
constexpr uint8_t BtpChargeSet = 0x36;
constexpr uint8_t OperationStatus = 0x3A;
}

namespace Bq27220Control {
constexpr uint16_t ControlStatus = 0x0000;
constexpr uint16_t DeviceType = 0x0001;
constexpr uint16_t FirmwareVersion = 0x0002;
constexpr uint16_t HardwareVersion = 0x0003;
constexpr uint16_t SetSnooze = 0x0013;
constexpr uint16_t ClearSnooze = 0x0014;
constexpr uint16_t Reset = 0x0041;
}

bool bq27220_probe(i2c_master_dev_handle_t i2c_dev);
bool bq27220_read_word(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t& value);
bool bq27220_write_word(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t value);
bool bq27220_control(i2c_master_dev_handle_t i2c_dev, uint16_t subcommand, uint16_t* response = nullptr);
bool bq27220_read_voltage_mv(i2c_master_dev_handle_t i2c_dev, uint16_t& voltage_mv);
bool bq27220_read_current_ma(i2c_master_dev_handle_t i2c_dev, int16_t& current_ma);
bool bq27220_read_average_current_ma(i2c_master_dev_handle_t i2c_dev, int16_t& current_ma);
bool bq27220_read_remaining_capacity_mah(i2c_master_dev_handle_t i2c_dev, uint16_t& capacity_mah);
bool bq27220_read_full_charge_capacity_mah(i2c_master_dev_handle_t i2c_dev, uint16_t& capacity_mah);
bool bq27220_read_state_of_charge(i2c_master_dev_handle_t i2c_dev, uint16_t& percent);
bool bq27220_read_state_of_health(i2c_master_dev_handle_t i2c_dev, uint16_t& percent);
bool bq27220_read_cycle_count(i2c_master_dev_handle_t i2c_dev, uint16_t& cycles);
bool bq27220_read_temperature_c(i2c_master_dev_handle_t i2c_dev, float& temperature_c);
bool bq27220_read_battery_status(i2c_master_dev_handle_t i2c_dev, uint16_t& status_bits);
bool bq27220_read_operation_status(i2c_master_dev_handle_t i2c_dev, operation_status_t& operation_status);
bool bq27220_read_data(i2c_master_dev_handle_t i2c_dev, Bq27220Data_t& data);
