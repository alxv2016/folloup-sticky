#include "bq27220.h"

#include <cstdlib>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "priv_include/bq27220_reg.h"

namespace {

static constexpr const char* TAG = "bq27220";
static constexpr uint16_t BQ27220_DEVICE_ID = 0x0220;
static constexpr int kI2cTimeoutMs = 50;

struct bq27220_data_t {
    i2c_master_dev_handle_t i2c_dev = nullptr;
};

enum bq27220_data_memory_name_t {
    BQ_DM_GAUGING_CONFIGURATION,
    BQ_DM_FULL_CHARGE_CAPACITY,
    BQ_DM_DESIGN_CAPACITY,
    BQ_DM_NEAR_FULL,
    BQ_DM_SELF_DISCHARGE_RATE,
    BQ_DM_RESERVE_CAPACITY,
    BQ_DM_EMF,
    BQ_DM_C0,
    BQ_DM_R0,
    BQ_DM_T0,
    BQ_DM_R1,
    BQ_DM_TC,
    BQ_DM_C1,
    BQ_DM_FIXED_EDV_0,
    BQ_DM_FIXED_EDV_1,
    BQ_DM_FIXED_EDV_2,
    BQ_DM_VOLTAGE_0_DOD,
    BQ_DM_VOLTAGE_10_DOD,
    BQ_DM_VOLTAGE_20_DOD,
    BQ_DM_VOLTAGE_30_DOD,
    BQ_DM_VOLTAGE_40_DOD,
    BQ_DM_VOLTAGE_50_DOD,
    BQ_DM_VOLTAGE_60_DOD,
    BQ_DM_VOLTAGE_70_DOD,
    BQ_DM_VOLTAGE_80_DOD,
    BQ_DM_VOLTAGE_90_DOD,
    BQ_DM_VOLTAGE_100_DOD,
};

struct bq27220_data_memory_entry_t {
    uint16_t address;
    bq27220_data_memory_name_t name;
};

static constexpr bq27220_data_memory_entry_t kDmTable[] = {
    {0x929B, BQ_DM_GAUGING_CONFIGURATION},
    {0x929D, BQ_DM_FULL_CHARGE_CAPACITY},
    {0x929F, BQ_DM_DESIGN_CAPACITY},
    {0x926B, BQ_DM_NEAR_FULL},
    {0x9268, BQ_DM_SELF_DISCHARGE_RATE},
    {0x926D, BQ_DM_RESERVE_CAPACITY},
    {0x92A7, BQ_DM_EMF},
    {0x92A9, BQ_DM_C0},
    {0x92AB, BQ_DM_R0},
    {0x92AD, BQ_DM_T0},
    {0x92AF, BQ_DM_R1},
    {0x92B1, BQ_DM_TC},
    {0x92B2, BQ_DM_C1},
    {0x92B4, BQ_DM_FIXED_EDV_0},
    {0x92B7, BQ_DM_FIXED_EDV_1},
    {0x92BA, BQ_DM_FIXED_EDV_2},
    {0x92BD, BQ_DM_VOLTAGE_0_DOD},
    {0x92BF, BQ_DM_VOLTAGE_10_DOD},
    {0x92C1, BQ_DM_VOLTAGE_20_DOD},
    {0x92C3, BQ_DM_VOLTAGE_30_DOD},
    {0x92C5, BQ_DM_VOLTAGE_40_DOD},
    {0x92C7, BQ_DM_VOLTAGE_50_DOD},
    {0x92C9, BQ_DM_VOLTAGE_60_DOD},
    {0x92CB, BQ_DM_VOLTAGE_70_DOD},
    {0x92CD, BQ_DM_VOLTAGE_80_DOD},
    {0x92CF, BQ_DM_VOLTAGE_90_DOD},
    {0x92D1, BQ_DM_VOLTAGE_100_DOD},
};

void delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

esp_err_t i2c_read_bytes(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint8_t* data, size_t len)
{
    ESP_RETURN_ON_FALSE(i2c_dev != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid i2c device");
    ESP_RETURN_ON_FALSE(data != nullptr && len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid read buffer");

    const uint8_t reg_buf = reg;
    return i2c_master_transmit_receive(i2c_dev, &reg_buf, sizeof(reg_buf), data, len, kI2cTimeoutMs);
}

esp_err_t i2c_write_bytes(i2c_master_dev_handle_t i2c_dev, uint8_t reg, const uint8_t* data, size_t len)
{
    ESP_RETURN_ON_FALSE(i2c_dev != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid i2c device");
    ESP_RETURN_ON_FALSE(data != nullptr && len > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid write buffer");

    uint8_t reg_buf = reg;
    i2c_master_transmit_multi_buffer_info_t tx_bufs[2] = {
        {
            .write_buffer = &reg_buf,
            .buffer_size = sizeof(reg_buf),
        },
        {
            .write_buffer = const_cast<uint8_t*>(data),
            .buffer_size = len,
        },
    };
    return i2c_master_multi_buffer_transmit(i2c_dev, tx_bufs, 2, kI2cTimeoutMs);
}

esp_err_t read_u16(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t* value)
{
    if(value == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[2] = {};
    ESP_RETURN_ON_ERROR(i2c_read_bytes(i2c_dev, reg, raw, sizeof(raw)), TAG, "i2c read failed");
    *value = static_cast<uint16_t>(raw[0]) | static_cast<uint16_t>(static_cast<uint16_t>(raw[1]) << 8);
    return ESP_OK;
}

esp_err_t write_u16(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t value)
{
    const uint8_t raw[2] = {
        static_cast<uint8_t>(value & 0xFFU),
        static_cast<uint8_t>((value >> 8) & 0xFFU),
    };
    return i2c_write_bytes(i2c_dev, reg, raw, sizeof(raw));
}

uint16_t bq27220_read_u16(bq27220_handle_t bq_handle, uint8_t reg)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, 0, TAG, "Invalid handle");
    auto* bq_data = static_cast<bq27220_data_t*>(bq_handle);
    ESP_RETURN_ON_FALSE(bq_data->i2c_dev != nullptr, 0, TAG, "Invalid i2c device");

    uint16_t value = 0;
    const esp_err_t err = read_u16(bq_data->i2c_dev, reg, &value);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "(%s) read reg 0x%02X failed: %s", __func__, reg, esp_err_to_name(err));
        return 0;
    }

    return value;
}

esp_err_t bq27220_write_u16(bq27220_handle_t bq_handle, uint8_t reg, uint16_t value)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    auto* bq_data = static_cast<bq27220_data_t*>(bq_handle);
    ESP_RETURN_ON_FALSE(bq_data->i2c_dev != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid i2c device");
    return write_u16(bq_data->i2c_dev, reg, value);
}

esp_err_t bq27220_control_raw(bq27220_handle_t bq_handle, uint16_t control)
{
    return bq27220_write_u16(bq_handle, COMMAND_CONTROL, control);
}

uint8_t bq27220_get_checksum(uint8_t* data, uint16_t len)
{
    ESP_RETURN_ON_FALSE(data != nullptr, 0, TAG, "Invalid data");

    uint8_t ret = 0;
    for(uint16_t i = 0; i < len; ++i)
    {
        ret = static_cast<uint8_t>(ret + data[i]);
    }
    return static_cast<uint8_t>(0xFFU - ret);
}

uint16_t bq27220_get_device_number(bq27220_handle_t bq_handle)
{
    bq27220_control_raw(bq_handle, CONTROL_DEVICE_NUMBER);
    delay_ms(15);
    return bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
}

bool wait_cfgupdate_state(bq27220_handle_t bq_handle, bool expected)
{
    operation_status_t status = {};
    uint32_t timeout = 20;

    while(timeout-- > 0)
    {
        if(bq27220_get_operation_status(bq_handle, &status) == ESP_OK &&
           static_cast<bool>(status.CFGUPDATE) == expected)
        {
            return true;
        }
        delay_ms(100);
    }

    return false;
}

esp_err_t write_profile_entry(bq27220_handle_t bq_handle, uint16_t address, uint16_t value, const char* label)
{
    const esp_err_t err = bq27220_set_parameter_u16(bq_handle, address, value);
    if(err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write %s (0x%04X): %s", label, address, esp_err_to_name(err));
    }
    return err;
}

} // namespace

bq27220_handle_t bq27220_create(const bq27220_config_t* config)
{
    ESP_RETURN_ON_FALSE(config != nullptr, nullptr, TAG, "Invalid config");
    ESP_RETURN_ON_FALSE(config->i2c_dev != nullptr, nullptr, TAG, "Invalid i2c device");
    ESP_RETURN_ON_FALSE(config->cedv != nullptr, nullptr, TAG, "Invalid cedv");
    ESP_RETURN_ON_FALSE(config->cfg != nullptr, nullptr, TAG, "Invalid gauging config");

    auto* handle = static_cast<bq27220_data_t*>(calloc(1, sizeof(bq27220_data_t)));
    if(handle == nullptr)
    {
        ESP_LOGE(TAG, "Memory allocation failed");
        return nullptr;
    }

    handle->i2c_dev = config->i2c_dev;

    uint16_t data = bq27220_get_device_number(handle);
    if(data != BQ27220_DEVICE_ID)
    {
        ESP_LOGE(TAG, "Invalid Device Number %04X != 0x%04X", data, BQ27220_DEVICE_ID);
        free(handle);
        return nullptr;
    }

    data = bq27220_get_fw_version(handle);
    ESP_LOGI(TAG, "Firmware Version %04X", data);
    data = bq27220_get_hw_version(handle);
    ESP_LOGI(TAG, "Hardware Version %04X", data);

    if(bq27220_unseal(handle) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unseal");
        free(handle);
        return nullptr;
    }

    uint16_t design_cap = bq27220_get_design_capacity(handle);
    const uint16_t emf = bq27220_get_parameter_u16(handle, kDmTable[BQ_DM_EMF].address);
    const uint16_t t0 = bq27220_get_parameter_u16(handle, kDmTable[BQ_DM_T0].address);
    const uint16_t dod20 = bq27220_get_parameter_u16(handle, kDmTable[BQ_DM_VOLTAGE_20_DOD].address);

    const parameter_cedv_t* cedv = config->cedv;
    ESP_LOGI(TAG, "Design Capacity: %u, EMF: %u, T0: %u, DOD20: %u", design_cap, emf, t0, dod20);

    if(cedv->design_cap == design_cap && cedv->EMF == emf && cedv->T0 == t0 && cedv->DOD20 == dod20)
    {
        ESP_LOGI(TAG, "Skip battery profile update");
        return handle;
    }

    ESP_LOGW(TAG, "Start updating battery profile");
    ESP_LOGI(TAG, "Entering CFG_UPDATE mode...");
    if(bq27220_control_raw(handle, CONTROL_ENTER_CFG_UPDATE) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to enter CFG_UPDATE mode");
        free(handle);
        return nullptr;
    }

    // TRM suggests waiting >= 1100ms before checking CFGUPDATE.
    delay_ms(1200);

    if(!wait_cfgupdate_state(handle, true))
    {
        ESP_LOGE(TAG, "Battery profile update failed - CFGUPDATE timeout");
        free(handle);
        return nullptr;
    }

    ESP_LOGI(TAG, "CFG_UPDATE mode confirmed, writing parameters...");

    if(write_profile_entry(handle, kDmTable[BQ_DM_GAUGING_CONFIGURATION].address, config->cfg->full, "GaugingConfig") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_FULL_CHARGE_CAPACITY].address, cedv->full_charge_cap, "FullChargeCap") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_DESIGN_CAPACITY].address, cedv->design_cap, "DesignCap") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_NEAR_FULL].address, cedv->near_full, "NearFull") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_SELF_DISCHARGE_RATE].address, cedv->self_discharge_rate, "SelfDischargeRate") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_RESERVE_CAPACITY].address, cedv->reserve_cap, "ReserveCap") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_EMF].address, cedv->EMF, "EMF") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_C0].address, cedv->C0, "C0") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_R0].address, cedv->R0, "R0") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_T0].address, cedv->T0, "T0") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_R1].address, cedv->R1, "R1") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_TC].address, static_cast<uint16_t>((static_cast<uint16_t>(cedv->TC) << 8) | cedv->C1), "TC_C1") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_0_DOD].address, cedv->DOD0, "DOD0") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_10_DOD].address, cedv->DOD10, "DOD10") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_20_DOD].address, cedv->DOD20, "DOD20") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_30_DOD].address, cedv->DOD30, "DOD30") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_40_DOD].address, cedv->DOD40, "DOD40") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_50_DOD].address, cedv->DOD50, "DOD50") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_60_DOD].address, cedv->DOD60, "DOD60") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_70_DOD].address, cedv->DOD70, "DOD70") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_80_DOD].address, cedv->DOD80, "DOD80") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_90_DOD].address, cedv->DOD90, "DOD90") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_VOLTAGE_100_DOD].address, cedv->DOD100, "DOD100") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_FIXED_EDV_0].address, cedv->EDV0, "EDV0") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_FIXED_EDV_1].address, cedv->EDV1, "EDV1") != ESP_OK ||
       write_profile_entry(handle, kDmTable[BQ_DM_FIXED_EDV_2].address, cedv->EDV2, "EDV2") != ESP_OK)
    {
        ESP_LOGW(TAG, "Parameter write failed, exiting CFG_UPDATE mode without reinit...");
        bq27220_control_raw(handle, CONTROL_EXIT_CFG_UPDATE);
        delay_ms(10);
        free(handle);
        return nullptr;
    }

    ESP_LOGI(TAG, "All parameters written, exiting CFG_UPDATE mode...");
    if(bq27220_control_raw(handle, CONTROL_EXIT_CFG_UPDATE_REINIT) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to exit CFG_UPDATE mode with reinit");
        free(handle);
        return nullptr;
    }
    delay_ms(10);

    if(!wait_cfgupdate_state(handle, false))
    {
        ESP_LOGE(TAG, "Battery profile update failed - exit CFGUPDATE timeout");
        free(handle);
        return nullptr;
    }

    ESP_LOGI(TAG, "Waiting for device reinitialization...");
    delay_ms(2000);

    design_cap = bq27220_get_design_capacity(handle);
    ESP_LOGI(TAG, "Verification: read Design Cap = %u, expected = %u", design_cap, cedv->design_cap);

    if(cedv->design_cap == design_cap)
    {
        ESP_LOGI(TAG, "Battery profile update success");
    }
    else
    {
        ESP_LOGW(TAG, "Battery profile verification mismatch (read %u != %u)", design_cap, cedv->design_cap);
        ESP_LOGW(TAG, "Continuing with device current configuration");
    }

    bq27220_seal(handle);
    return handle;
}

esp_err_t bq27220_delete(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    free(bq_handle);
    return ESP_OK;
}

uint16_t bq27220_get_fw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, 0, TAG, "Invalid handle");
    bq27220_control_raw(bq_handle, CONTROL_FW_VERSION);
    delay_ms(15);
    return bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
}

uint16_t bq27220_get_hw_version(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, 0, TAG, "Invalid handle");
    bq27220_control_raw(bq_handle, CONTROL_HW_VERSION);
    delay_ms(15);
    return bq27220_read_u16(bq_handle, COMMAND_MAC_DATA);
}

esp_err_t bq27220_set_parameter_u16(bq27220_handle_t bq_handle, uint16_t address, uint16_t value)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");
    auto* bq_data = static_cast<bq27220_data_t*>(bq_handle);

    uint8_t buffer[4] = {};
    buffer[0] = static_cast<uint8_t>(address & 0xFFU);
    buffer[1] = static_cast<uint8_t>((address >> 8) & 0xFFU);
    buffer[2] = static_cast<uint8_t>((value >> 8) & 0xFFU);
    buffer[3] = static_cast<uint8_t>(value & 0xFFU);

    ESP_RETURN_ON_ERROR(i2c_write_bytes(bq_data->i2c_dev, COMMAND_SELECT_SUBCLASS, buffer, sizeof(buffer)),
                        TAG, "write parameter failed");
    delay_ms(10);

    const uint8_t checksum = bq27220_get_checksum(buffer, sizeof(buffer));
    const uint8_t commit_buf[2] = { checksum, 6 };
    ESP_RETURN_ON_ERROR(i2c_write_bytes(bq_data->i2c_dev, COMMAND_MAC_DATA_SUM, commit_buf, sizeof(commit_buf)),
                        TAG, "write checksum failed");
    delay_ms(10);

    return ESP_OK;
}

uint16_t bq27220_get_parameter_u16(bq27220_handle_t bq_handle, uint16_t address)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, 0, TAG, "Invalid handle");
    auto* bq_data = static_cast<bq27220_data_t*>(bq_handle);

    uint8_t buffer[2] = {
        static_cast<uint8_t>(address & 0xFFU),
        static_cast<uint8_t>((address >> 8) & 0xFFU),
    };

    if(i2c_write_bytes(bq_data->i2c_dev, COMMAND_SELECT_SUBCLASS, buffer, sizeof(buffer)) != ESP_OK)
    {
        return 0;
    }
    delay_ms(10);

    if(i2c_read_bytes(bq_data->i2c_dev, COMMAND_MAC_DATA, buffer, sizeof(buffer)) != ESP_OK)
    {
        return 0;
    }

    return static_cast<uint16_t>(static_cast<uint16_t>(buffer[0]) << 8) | static_cast<uint16_t>(buffer[1]);
}

esp_err_t bq27220_seal(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    operation_status_t status = {};
    ESP_RETURN_ON_ERROR(bq27220_get_operation_status(bq_handle, &status), TAG, "read operation status failed");
    if(status.SEC == OPERATION_STATUS_SEC_SEALED)
    {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bq27220_control_raw(bq_handle, CONTROL_SEALED), TAG, "seal command failed");
    delay_ms(10);
    ESP_RETURN_ON_ERROR(bq27220_get_operation_status(bq_handle, &status), TAG, "read operation status failed");
    return (status.SEC == OPERATION_STATUS_SEC_SEALED) ? ESP_OK : ESP_FAIL;
}

esp_err_t bq27220_unseal(bq27220_handle_t bq_handle)
{
    ESP_RETURN_ON_FALSE(bq_handle != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid handle");

    operation_status_t status = {};
    ESP_RETURN_ON_ERROR(bq27220_get_operation_status(bq_handle, &status), TAG, "read operation status failed");
    if(status.SEC == OPERATION_STATUS_SEC_UNSEALED)
    {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bq27220_control_raw(bq_handle, UNSEALKEY1), TAG, "unseal key1 failed");
    delay_ms(10);
    ESP_RETURN_ON_ERROR(bq27220_control_raw(bq_handle, UNSEALKEY2), TAG, "unseal key2 failed");
    delay_ms(10);
    ESP_RETURN_ON_ERROR(bq27220_get_operation_status(bq_handle, &status), TAG, "read operation status failed");
    return (status.SEC == OPERATION_STATUS_SEC_UNSEALED) ? ESP_OK : ESP_FAIL;
}

uint16_t bq27220_get_voltage(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_VOLTAGE);
}

int16_t bq27220_get_current(bq27220_handle_t bq_handle)
{
    return static_cast<int16_t>(bq27220_read_u16(bq_handle, COMMAND_CURRENT));
}

int16_t bq27220_get_avgcurrent(bq27220_handle_t bq_handle)
{
    return static_cast<int16_t>(bq27220_read_u16(bq_handle, COMMAND_AVERAGE_CURRENT));
}

esp_err_t bq27220_get_battery_status(bq27220_handle_t bq_handle, battery_status_t* battery_status)
{
    ESP_RETURN_ON_FALSE(battery_status != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid battery_status");
    battery_status->full = bq27220_read_u16(bq_handle, COMMAND_BATTERY_STATUS);
    return ESP_OK;
}

esp_err_t bq27220_get_operation_status(bq27220_handle_t bq_handle, operation_status_t* operation_status)
{
    ESP_RETURN_ON_FALSE(operation_status != nullptr, ESP_ERR_INVALID_ARG, TAG, "Invalid operation_status");
    *reinterpret_cast<uint16_t*>(operation_status) = bq27220_read_u16(bq_handle, COMMAND_OPERATION_STATUS);
    return ESP_OK;
}

uint16_t bq27220_get_temperature(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TEMPERATURE);
}

uint16_t bq27220_get_cycle_count(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CYCLE_COUNT);
}

uint16_t bq27220_get_full_charge_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_FULL_CHARGE_CAPACITY);
}

uint16_t bq27220_get_design_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_DESIGN_CAPACITY);
}

uint16_t bq27220_get_remaining_capacity(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_REMAINING_CAPACITY);
}

uint16_t bq27220_get_state_of_charge(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_STATE_OF_CHARGE);
}

uint16_t bq27220_get_state_of_health(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_STATE_OF_HEALTH);
}

uint16_t bq27220_get_charge_voltage(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CHARGE_VOLTAGE);
}

uint16_t bq27220_get_charge_current(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_CHARGE_CURRENT);
}

int16_t bq27220_get_average_power(bq27220_handle_t bq_handle)
{
    return static_cast<int16_t>(bq27220_read_u16(bq_handle, COMMAND_AVERAGE_POWER));
}

uint16_t bq27220_get_time_to_empty(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TIME_TO_EMPTY);
}

uint16_t bq27220_get_time_to_full(bq27220_handle_t bq_handle)
{
    return bq27220_read_u16(bq_handle, COMMAND_TIME_TO_FULL);
}

int16_t bq27220_get_maxload_current(bq27220_handle_t bq_handle)
{
    return static_cast<int16_t>(bq27220_read_u16(bq_handle, COMMAND_MAX_LOAD_CURRENT));
}

int16_t bq27220_get_standby_current(bq27220_handle_t bq_handle)
{
    return static_cast<int16_t>(bq27220_read_u16(bq_handle, COMMAND_STANDBY_CURRENT));
}

bool bq27220_probe(i2c_master_dev_handle_t i2c_dev)
{
    uint16_t device_id = 0;
    if(!bq27220_control(i2c_dev, Bq27220Control::DeviceType, &device_id))
    {
        return false;
    }
    return device_id == BQ27220_DEVICE_ID;
}

bool bq27220_read_word(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t& value)
{
    return read_u16(i2c_dev, reg, &value) == ESP_OK;
}

bool bq27220_write_word(i2c_master_dev_handle_t i2c_dev, uint8_t reg, uint16_t value)
{
    return write_u16(i2c_dev, reg, value) == ESP_OK;
}

bool bq27220_control(i2c_master_dev_handle_t i2c_dev, uint16_t subcommand, uint16_t* response)
{
    if(!bq27220_write_word(i2c_dev, Bq27220Reg::Control, subcommand))
    {
        return false;
    }

    if(response == nullptr)
    {
        return true;
    }

    delay_ms(15);
    return bq27220_read_word(i2c_dev, COMMAND_MAC_DATA, *response);
}

bool bq27220_read_voltage_mv(i2c_master_dev_handle_t i2c_dev, uint16_t& voltage_mv)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::Voltage, voltage_mv);
}

bool bq27220_read_current_ma(i2c_master_dev_handle_t i2c_dev, int16_t& current_ma)
{
    uint16_t raw = 0;
    if(!bq27220_read_word(i2c_dev, Bq27220Reg::Current, raw))
    {
        return false;
    }
    current_ma = static_cast<int16_t>(raw);
    return true;
}

bool bq27220_read_average_current_ma(i2c_master_dev_handle_t i2c_dev, int16_t& current_ma)
{
    uint16_t raw = 0;
    if(!bq27220_read_word(i2c_dev, Bq27220Reg::AverageCurrent, raw))
    {
        return false;
    }
    current_ma = static_cast<int16_t>(raw);
    return true;
}

bool bq27220_read_remaining_capacity_mah(i2c_master_dev_handle_t i2c_dev, uint16_t& capacity_mah)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::RemainingCapacity, capacity_mah);
}

bool bq27220_read_full_charge_capacity_mah(i2c_master_dev_handle_t i2c_dev, uint16_t& capacity_mah)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::FullChargeCapacity, capacity_mah);
}

bool bq27220_read_state_of_charge(i2c_master_dev_handle_t i2c_dev, uint16_t& percent)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::StateOfCharge, percent);
}

bool bq27220_read_state_of_health(i2c_master_dev_handle_t i2c_dev, uint16_t& percent)
{
    uint16_t raw = 0;
    if(!bq27220_read_word(i2c_dev, Bq27220Reg::StateOfHealth, raw))
    {
        return false;
    }
    percent = static_cast<uint16_t>(raw & 0x00FFU);
    return true;
}

bool bq27220_read_cycle_count(i2c_master_dev_handle_t i2c_dev, uint16_t& cycles)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::CycleCount, cycles);
}

bool bq27220_read_temperature_c(i2c_master_dev_handle_t i2c_dev, float& temperature_c)
{
    uint16_t raw = 0;
    if(!bq27220_read_word(i2c_dev, Bq27220Reg::Temperature, raw))
    {
        return false;
    }

    temperature_c = static_cast<float>(raw) * 0.1f - 273.15f;
    return true;
}

bool bq27220_read_battery_status(i2c_master_dev_handle_t i2c_dev, uint16_t& status_bits)
{
    return bq27220_read_word(i2c_dev, Bq27220Reg::BatteryStatus, status_bits);
}

bool bq27220_read_operation_status(i2c_master_dev_handle_t i2c_dev, operation_status_t& operation_status)
{
    uint16_t raw = 0;
    if(!bq27220_read_word(i2c_dev, COMMAND_OPERATION_STATUS, raw))
    {
        return false;
    }

    *reinterpret_cast<uint16_t*>(&operation_status) = raw;
    return true;
}

bool bq27220_read_data(i2c_master_dev_handle_t i2c_dev, Bq27220Data_t& data)
{
    return bq27220_read_voltage_mv(i2c_dev, data.voltage_mv) &&
           bq27220_read_current_ma(i2c_dev, data.current_ma) &&
           bq27220_read_average_current_ma(i2c_dev, data.average_current_ma) &&
           bq27220_read_remaining_capacity_mah(i2c_dev, data.remaining_capacity_mah) &&
           bq27220_read_full_charge_capacity_mah(i2c_dev, data.full_charge_capacity_mah) &&
           bq27220_read_state_of_charge(i2c_dev, data.state_of_charge_percent) &&
           bq27220_read_state_of_health(i2c_dev, data.state_of_health_percent) &&
           bq27220_read_cycle_count(i2c_dev, data.cycle_count) &&
           bq27220_read_temperature_c(i2c_dev, data.temperature_c) &&
           bq27220_read_battery_status(i2c_dev, data.battery_status);
}
