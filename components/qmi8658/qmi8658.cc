#include "qmi8658.h"

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <cmath>
#include <cstring>

#include <esp_check.h>
#include <esp_log.h>

namespace {

constexpr const char* kTag = "Qmi8658";
constexpr UBaseType_t kInterruptTaskPriority = 2;
constexpr BaseType_t kInterruptTaskCore = 0;

constexpr uint8_t kRegisterWhoAmI = 0x00;
constexpr uint8_t kRegisterRevision = 0x01;
constexpr uint8_t kRegisterCtrl1 = 0x02;
constexpr uint8_t kRegisterCtrl2 = 0x03;
constexpr uint8_t kRegisterCtrl3 = 0x04;
constexpr uint8_t kRegisterCtrl5 = 0x06;
constexpr uint8_t kRegisterCtrl7 = 0x08;
constexpr uint8_t kRegisterCtrl8 = 0x09;
constexpr uint8_t kRegisterCtrl9 = 0x0A;
constexpr uint8_t kRegisterCal1L = 0x0B;
constexpr uint8_t kRegisterCal1H = 0x0C;
constexpr uint8_t kRegisterCal2L = 0x0D;
constexpr uint8_t kRegisterCal2H = 0x0E;
constexpr uint8_t kRegisterCal3L = 0x0F;
constexpr uint8_t kRegisterCal3H = 0x10;
constexpr uint8_t kRegisterCal4L = 0x11;
constexpr uint8_t kRegisterCal4H = 0x12;
constexpr uint8_t kRegisterFifoWatermark = 0x13;
constexpr uint8_t kRegisterFifoCtrl = 0x14;
constexpr uint8_t kRegisterFifoCount = 0x15;
constexpr uint8_t kRegisterFifoStatus = 0x16;
constexpr uint8_t kRegisterFifoData = 0x17;
constexpr uint8_t kRegisterStatusInt = 0x2D;
constexpr uint8_t kRegisterStatus0 = 0x2E;
constexpr uint8_t kRegisterStatus1 = 0x2F;
constexpr uint8_t kRegisterTimestamp = 0x30;
constexpr uint8_t kRegisterTemperature = 0x33;
constexpr uint8_t kRegisterAcceleration = 0x35;
constexpr uint8_t kRegisterGyroscope = 0x3B;
constexpr uint8_t kRegisterCodStatus = 0x46;
constexpr uint8_t kRegisterDqw = 0x49;
constexpr uint8_t kRegisterDvx = 0x51;
constexpr uint8_t kRegisterTapStatus = 0x59;
constexpr uint8_t kRegisterStepCount = 0x5A;
constexpr uint8_t kRegisterReset = 0x60;
constexpr uint8_t kRegisterResetResult = 0x4D;

constexpr uint8_t kExpectedWhoAmI = 0x05;
constexpr uint8_t kResetCommandValue = 0xB0;
constexpr uint8_t kResetDoneValue = 0x80;

constexpr uint8_t kCtrl1AddrAutoIncrement = 1 << 6;
constexpr uint8_t kCtrl8StatusIntCtrl9Handshake = 1 << 7;

constexpr uint8_t kCtrl7AccelEnable = 1 << 0;
constexpr uint8_t kCtrl7GyroEnable = 1 << 1;
constexpr uint8_t kCtrl7DisableAllSensors = 1 << 1;
constexpr uint8_t kCtrl7DisableDataReady = 1 << 5;
constexpr uint8_t kCtrl7SyncSampleMode = 1 << 7;

constexpr uint8_t kStatusIntCtrl9Done = 0x80;
constexpr uint8_t kStatusIntLocked = 0x02;
constexpr uint8_t kStatusIntAvailable = 0x01;
constexpr uint8_t kStatus0AccelReady = 0x01;
constexpr uint8_t kStatus0GyroReady = 0x02;

constexpr uint8_t kCtrl1Int1Enable = 1 << 3;
constexpr uint8_t kCtrl1Int2Enable = 1 << 4;
constexpr uint8_t kCtrl1FifoMapInt1 = 1 << 2;

constexpr uint8_t kFifoStatusNotEmpty = 1 << 4;
constexpr uint8_t kFifoStatusOverflow = 1 << 5;
constexpr uint8_t kFifoStatusWatermark = 1 << 6;
constexpr uint8_t kFifoStatusFull = 1 << 7;

constexpr uint8_t kAccelLpfMask = 0xF9;
constexpr uint8_t kGyroLpfMask = 0x9F;

constexpr size_t kInterruptQueueDepth = 8;
constexpr uint32_t kDefaultCtrl9TimeoutMs = 1000;

uint8_t LowByte(uint16_t value) {
    return static_cast<uint8_t>(value & 0xFF);
}

uint8_t HighByte(uint16_t value) {
    return static_cast<uint8_t>((value >> 8) & 0xFF);
}

}  // namespace

Qmi8658::Qmi8658(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : Qmi8658(i2c_bus, addr, Config{}) {}

Qmi8658::Qmi8658(i2c_master_bus_handle_t i2c_bus, uint8_t addr, const Config& config)
    : I2cDevice(i2c_bus, addr),
      config_(config),
      accelerometer_enabled_(config.enable_accelerometer),
      gyroscope_enabled_(config.enable_gyroscope),
      sample_mode_(config.sample_mode),
      acc_lsb_div_(ResolveAccLsbDiv()),
      gyro_lsb_div_(ResolveGyroLsbDiv()) {
    if (config_.interrupt2_pin != GPIO_NUM_NC) {
        interrupt2_isr_queue_ = xQueueCreate(kInterruptQueueDepth, sizeof(uint32_t));
        interrupt2_event_queue_ = xQueueCreate(kInterruptQueueDepth, sizeof(InterruptEvent));
    }
}

esp_err_t Qmi8658::Initialize(bool log_failures) {
    if (initialized_) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(Reset(true, 500), kTag, "failed to reset QMI8658");
    ESP_RETURN_ON_ERROR(ConfigureInterruptPins(), kTag, "failed to configure IMU GPIOs");
    ESP_RETURN_ON_ERROR(ConfigureDefaultState(), kTag, "failed to configure IMU defaults");

    uint8_t chip_id = 0;
    ESP_RETURN_ON_ERROR(ReadIdentity(&chip_id, nullptr, log_failures), kTag,
                        "failed to read IMU identity");
    if (chip_id != kExpectedWhoAmI) {
        if (log_failures) {
            ESP_LOGE(kTag, "unexpected QMI8658 chip id: 0x%02X", chip_id);
        }
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(CopyUsidAndRevision(), kTag, "failed to copy QMI8658 USID/revision");
    ESP_RETURN_ON_ERROR(
        ConfigAccelerometer(config_.acc_range, config_.acc_odr, config_.acc_lpf_mode), kTag,
        "failed to configure accelerometer");
    ESP_RETURN_ON_ERROR(
        ConfigGyroscope(config_.gyro_range, config_.gyro_odr, config_.gyro_lpf_mode), kTag,
        "failed to configure gyroscope");

    if (sample_mode_ == SampleMode::kSync) {
        ESP_RETURN_ON_ERROR(EnableSyncSampleMode(), kTag, "failed to enable sync sample mode");
    } else {
        ESP_RETURN_ON_ERROR(DisableSyncSampleMode(), kTag, "failed to disable sync sample mode");
    }

    if (config_.enable_wake_on_motion) {
        ESP_RETURN_ON_ERROR(
            ConfigWakeOnMotion(config_.wom_threshold_mg, config_.wom_odr, IntPin::kInt2, 1, 0x20),
            kTag, "failed to configure wake on motion");
    } else {
        if (config_.enable_fifo) {
            ESP_RETURN_ON_ERROR(
                ConfigFifo(config_.fifo_mode, config_.fifo_samples,
                           config_.interrupt2_pin != GPIO_NUM_NC ? IntPin::kInt2 : IntPin::kDisabled,
                           config_.fifo_watermark),
                kTag, "failed to configure FIFO");
        } else if (config_.enable_data_ready_interrupt) {
            ESP_RETURN_ON_ERROR(EnableDataReadyInterrupt(true), kTag,
                                "failed to enable data ready interrupt");
            if (config_.interrupt2_pin != GPIO_NUM_NC) {
                ESP_RETURN_ON_ERROR(EnableInt(IntPin::kInt2, true), kTag,
                                    "failed to route data ready interrupt to INT2");
            }
        }

        if (config_.enable_accelerometer) {
            EnableAccelerometer();
        } else {
            DisableAccelerometer();
        }
        if (config_.enable_gyroscope) {
            EnableGyroscope();
        } else {
            DisableGyroscope();
        }
    }

    if (config_.interrupt2_pin != GPIO_NUM_NC && interrupt_task_handle_ == nullptr) {
        BaseType_t created =
            xTaskCreatePinnedToCore(InterruptTaskEntry, "qmi8658_irq", 4096, this,
                                    kInterruptTaskPriority, &interrupt_task_handle_,
                                    kInterruptTaskCore);
        if (created != pdPASS) {
            ESP_LOGE(kTag, "failed to create IMU interrupt task");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!config_.enable_fifo) {
        SyncLatestSampleFromRegisters();
    }

    initialized_ = true;
    return ESP_OK;
}

esp_err_t Qmi8658::Reset(bool wait_result, uint32_t timeout_ms) {
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterReset, kResetCommandValue), kTag,
                        "failed to write QMI8658 reset command");
    if (wait_result) {
        TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeout_ms);
        while (xTaskGetTickCount() < deadline) {
            uint8_t value = 0;
            if (ReadReg(kRegisterResetResult, &value) == ESP_OK && value == kResetDoneValue) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    return WriteReg(kRegisterCtrl1, kCtrl1AddrAutoIncrement);
}

esp_err_t Qmi8658::ReadIdentity(uint8_t* chip_id, uint8_t* revision, bool log_failures) {
    if (chip_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = ReadReg(kRegisterWhoAmI, chip_id);
    if (err != ESP_OK) {
        if (log_failures) {
            ESP_LOGE(kTag, "failed to read IMU chip id: %s", esp_err_to_name(err));
        }
        return err;
    }
    if (revision != nullptr) {
        err = ReadReg(kRegisterRevision, revision);
        if (err != ESP_OK && log_failures) {
            ESP_LOGE(kTag, "failed to read IMU revision: %s", esp_err_to_name(err));
        }
    }
    return err;
}

esp_err_t Qmi8658::ReadTemperature(float* temperature_c) {
    if (temperature_c == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config_.enable_fifo) {
        std::lock_guard<std::mutex> lock(sample_mutex_);
        if (!latest_temperature_valid_) {
            return ESP_ERR_INVALID_STATE;
        }
        *temperature_c = latest_temperature_c_;
        return ESP_OK;
    }
    return ReadTemperatureRegister(temperature_c);
}

esp_err_t Qmi8658::ReadAcceleration(float acceleration_mg[3]) {
    if (acceleration_mg == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config_.enable_fifo) {
        Qmi8658Sample sample = {};
        ESP_RETURN_ON_ERROR(ReadLatestSample(&sample, nullptr), kTag,
                            "failed to read cached FIFO acceleration");
        std::memcpy(acceleration_mg, sample.acceleration_mg, sizeof(sample.acceleration_mg));
        return ESP_OK;
    }
    int16_t raw[3] = {};
    ESP_RETURN_ON_ERROR(ReadRawAxes(kRegisterAcceleration, raw), kTag,
                        "failed to read IMU acceleration");
    for (size_t i = 0; i < 3; ++i) {
        acceleration_mg[i] = ConvertAccToMg(raw[i], acc_lsb_div_);
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ReadGyroscope(float gyro_dps[3]) {
    if (gyro_dps == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config_.enable_fifo) {
        Qmi8658Sample sample = {};
        ESP_RETURN_ON_ERROR(ReadLatestSample(&sample, nullptr), kTag,
                            "failed to read cached FIFO gyroscope");
        std::memcpy(gyro_dps, sample.gyro_dps, sizeof(sample.gyro_dps));
        return ESP_OK;
    }
    int16_t raw[3] = {};
    ESP_RETURN_ON_ERROR(ReadRawAxes(kRegisterGyroscope, raw), kTag, "failed to read IMU gyroscope");
    for (size_t i = 0; i < 3; ++i) {
        gyro_dps[i] = ConvertGyroToDps(raw[i], gyro_lsb_div_);
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ReadSample(Qmi8658Sample* sample, bool read_timestamp) {
    if (sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config_.enable_fifo) {
        esp_err_t err = ReadLatestSample(sample, nullptr);
        if (err == ESP_OK && !read_timestamp) {
            sample->timestamp = 0;
        }
        return err;
    }

    if (read_timestamp) {
        sample->timestamp = GetTimestamp();
    } else {
        sample->timestamp = 0;
    }

    uint8_t buffer[12] = {};
    ESP_RETURN_ON_ERROR(ReadRegs(kRegisterAcceleration, buffer, sizeof(buffer)), kTag,
                        "failed to read IMU sample");

    for (size_t i = 0; i < 3; ++i) {
        int16_t raw_acc = static_cast<int16_t>((buffer[(i * 2) + 1] << 8) | buffer[i * 2]);
        int16_t raw_gyro =
            static_cast<int16_t>((buffer[(i * 2) + 7] << 8) | buffer[(i * 2) + 6]);
        sample->acceleration_mg[i] = ConvertAccToMg(raw_acc, acc_lsb_div_);
        sample->gyro_dps[i] = ConvertGyroToDps(raw_gyro, gyro_lsb_div_);
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ReadLatestSample(Qmi8658Sample* sample, float* temperature_c) {
    if (sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (config_.enable_fifo) {
        size_t bytes_read = 0;
        esp_err_t err = ReadFromFifo(&bytes_read);
        if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
            return err;
        }
    } else {
        ESP_RETURN_ON_ERROR(SyncLatestSampleFromRegisters(), kTag,
                            "failed to refresh live IMU sample");
    }

    std::lock_guard<std::mutex> lock(sample_mutex_);
    if (!latest_sample_valid_) {
        return ESP_ERR_INVALID_STATE;
    }
    *sample = latest_sample_;
    if (temperature_c != nullptr) {
        if (!latest_temperature_valid_) {
            return ESP_ERR_INVALID_STATE;
        }
        *temperature_c = latest_temperature_c_;
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ReadStatusInt(uint8_t* status) {
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ReadReg(kRegisterStatusInt, status);
}

esp_err_t Qmi8658::ReadStatus0(uint8_t* status) {
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ReadReg(kRegisterStatus0, status);
}

esp_err_t Qmi8658::ReadStatus1(uint8_t* status) {
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return ReadReg(kRegisterStatus1, status);
}

esp_err_t Qmi8658::ReadFromFifo(size_t* bytes_read) {
    if (bytes_read != nullptr) {
        *bytes_read = 0;
    }
    if (fifo_mode_value_ == 0 || fifo_mode_value_ == static_cast<uint8_t>(FifoMode::kBypass)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!accelerometer_enabled_ && !gyroscope_enabled_) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t fifo_status = 0;
    ESP_RETURN_ON_ERROR(ReadReg(kRegisterFifoStatus, &fifo_status), kTag,
                        "failed to read FIFO status");
    if ((fifo_status & kFifoStatusNotEmpty) == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if ((fifo_status & kFifoStatusOverflow) != 0) {
        if (config_.fifo_mode == FifoMode::kStream) {
            ESP_LOGI(kTag, "FIFO stream wrapped; oldest IMU samples were overwritten");
        } else {
            ESP_LOGW(kTag, "FIFO overflow condition detected");
        }
    }

    uint8_t count_and_status[2] = {};
    ESP_RETURN_ON_ERROR(ReadRegs(kRegisterFifoCount, count_and_status, sizeof(count_and_status)),
                        kTag, "failed to read FIFO count/status");
    size_t fifo_bytes = 2U * static_cast<size_t>(((count_and_status[1] & 0x03) << 8) |
                                                 count_and_status[0]);
    ESP_LOGD(kTag, "FIFO status=0x%02X bytes_available=%u frame_bytes=%u", fifo_status,
             static_cast<unsigned>(fifo_bytes), static_cast<unsigned>(ResolveFifoFrameBytes()));
    if (fifo_bytes == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    fifo_buffer_.resize(fifo_bytes);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kRequestFifo, kDefaultCtrl9TimeoutMs, true), kTag,
                        "failed to request FIFO read mode");
    esp_err_t read_err = ReadRegs(kRegisterFifoData, fifo_buffer_.data(), fifo_buffer_.size());
    esp_err_t clear_err = WriteReg(kRegisterFifoCtrl, fifo_mode_value_);
    if (read_err != ESP_OK) {
        return read_err;
    }
    if (clear_err != ESP_OK) {
        return clear_err;
    }

    bool updated_sample = false;
    ESP_RETURN_ON_ERROR(DecodeFifoBuffer(fifo_bytes, &updated_sample), kTag,
                        "failed to decode FIFO buffer");
    if (bytes_read != nullptr) {
        *bytes_read = fifo_bytes;
    }
    return updated_sample ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool Qmi8658::GetDataReady() {
    if ((irq_enable_mask_ & 0x03) != 0 && config_.interrupt2_pin != GPIO_NUM_NC &&
        GetInterrupt2Level() == 0) {
        return false;
    }

    if (sample_mode_ == SampleMode::kSync) {
        uint8_t status_int = 0;
        return ReadStatusInt(&status_int) == ESP_OK && (status_int & kStatusIntLocked) != 0;
    }

    uint8_t status0 = 0;
    if (ReadStatus0(&status0) != ESP_OK) {
        return false;
    }
    if (accelerometer_enabled_ && gyroscope_enabled_) {
        return (status0 & (kStatus0AccelReady | kStatus0GyroReady)) != 0;
    }
    if (gyroscope_enabled_) {
        return (status0 & kStatus0GyroReady) != 0;
    }
    if (accelerometer_enabled_) {
        return (status0 & kStatus0AccelReady) != 0;
    }
    return false;
}

uint32_t Qmi8658::GetTimestamp() {
    uint8_t timestamp_bytes[3] = {};
    if (ReadRegs(kRegisterTimestamp, timestamp_bytes, sizeof(timestamp_bytes)) != ESP_OK) {
        return last_timestamp_;
    }
    uint32_t raw_timestamp = static_cast<uint32_t>(timestamp_bytes[0]) |
                             (static_cast<uint32_t>(timestamp_bytes[1]) << 8) |
                             (static_cast<uint32_t>(timestamp_bytes[2]) << 16);
    if (raw_timestamp >= last_timestamp_) {
        last_timestamp_ = raw_timestamp;
    } else {
        last_timestamp_ = raw_timestamp + 0x1000000U - last_timestamp_;
    }
    return last_timestamp_;
}

int Qmi8658::GetInterrupt2Level() const {
    if (config_.interrupt2_pin == GPIO_NUM_NC) {
        return -1;
    }
    return gpio_get_level(config_.interrupt2_pin);
}

void Qmi8658::SetInterrupt2Callback(InterruptCallback callback) {
    interrupt2_callback_ = std::move(callback);
}

void Qmi8658::SetFifoSampleCallback(FifoSampleCallback callback) {
    fifo_sample_callback_ = std::move(callback);
}

esp_err_t Qmi8658::EnableInt(IntPin pin, bool enable) {
    switch (pin) {
        case IntPin::kInt1:
            irq_enable_mask_ = enable ? static_cast<uint8_t>(irq_enable_mask_ | 0x01)
                                       : static_cast<uint8_t>(irq_enable_mask_ & 0xFE);
            return enable ? SetRegisterBit(kRegisterCtrl1, 3) : ClearRegisterBit(kRegisterCtrl1, 3);
        case IntPin::kInt2:
            irq_enable_mask_ = enable ? static_cast<uint8_t>(irq_enable_mask_ | 0x02)
                                       : static_cast<uint8_t>(irq_enable_mask_ & 0xFD);
            return enable ? SetRegisterBit(kRegisterCtrl1, 4) : ClearRegisterBit(kRegisterCtrl1, 4);
        case IntPin::kDisabled:
            return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t Qmi8658::EnableDataReadyInterrupt(bool enable) {
    return enable ? ClearRegisterBit(kRegisterCtrl7, 5) : SetRegisterBit(kRegisterCtrl7, 5);
}

esp_err_t Qmi8658::ConfigAccelerometer(AccRange range, AccOdr odr, LpfMode lpf_mode) {
    bool was_enabled = AccelerometerEnabled();
    if (was_enabled) {
        DisableAccelerometer();
    }

    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl2, 0x8F,
                                           static_cast<uint8_t>(static_cast<uint8_t>(range) << 4)),
                        kTag, "failed to set accelerometer range");
    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl2, 0xF0, static_cast<uint8_t>(odr)), kTag,
                        "failed to set accelerometer ODR");

    if (lpf_mode != LpfMode::kOff) {
        ESP_RETURN_ON_ERROR(
            ReadModifyRegister(kRegisterCtrl5, kAccelLpfMask,
                               static_cast<uint8_t>(static_cast<uint8_t>(lpf_mode) << 1)),
            kTag, "failed to set accelerometer LPF");
        ESP_RETURN_ON_ERROR(SetRegisterBit(kRegisterCtrl5, 0), kTag,
                            "failed to enable accelerometer LPF");
    } else {
        ESP_RETURN_ON_ERROR(ClearRegisterBit(kRegisterCtrl5, 0), kTag,
                            "failed to disable accelerometer LPF");
    }

    config_.acc_range = range;
    config_.acc_odr = odr;
    config_.acc_lpf_mode = lpf_mode;
    acc_lsb_div_ = ResolveAccLsbDiv();

    if (was_enabled) {
        EnableAccelerometer();
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ConfigGyroscope(GyroRange range, GyroOdr odr, LpfMode lpf_mode) {
    bool was_enabled = GyroscopeEnabled();
    if (was_enabled) {
        DisableGyroscope();
    }

    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl3, 0x8F,
                                           static_cast<uint8_t>(static_cast<uint8_t>(range) << 4)),
                        kTag, "failed to set gyroscope range");
    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl3, 0xF0, static_cast<uint8_t>(odr)), kTag,
                        "failed to set gyroscope ODR");

    if (lpf_mode != LpfMode::kOff) {
        ESP_RETURN_ON_ERROR(
            ReadModifyRegister(kRegisterCtrl5, kGyroLpfMask,
                               static_cast<uint8_t>(static_cast<uint8_t>(lpf_mode) << 5)),
            kTag, "failed to set gyroscope LPF");
        ESP_RETURN_ON_ERROR(SetRegisterBit(kRegisterCtrl5, 4), kTag,
                            "failed to enable gyroscope LPF");
    } else {
        ESP_RETURN_ON_ERROR(ClearRegisterBit(kRegisterCtrl5, 4), kTag,
                            "failed to disable gyroscope LPF");
    }

    config_.gyro_range = range;
    config_.gyro_odr = odr;
    config_.gyro_lpf_mode = lpf_mode;
    gyro_lsb_div_ = ResolveGyroLsbDiv();

    if (was_enabled) {
        EnableGyroscope();
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ConfigFifo(FifoMode mode, FifoSamples samples, IntPin pin,
                              uint8_t trigger_samples) {
    bool gyro_was_enabled = GyroscopeEnabled();
    bool accel_was_enabled = AccelerometerEnabled();
    if (gyro_was_enabled) {
        DisableGyroscope();
    }
    if (accel_was_enabled) {
        DisableAccelerometer();
    }

    ESP_RETURN_ON_ERROR(WriteCommand(Command::kResetFifo, kDefaultCtrl9TimeoutMs, true), kTag,
                        "failed to reset FIFO");

    fifo_interrupt_enabled_ = pin != IntPin::kDisabled;
    fifo_interrupt_pin_ = pin;
    switch (pin) {
        case IntPin::kInt1:
            ESP_RETURN_ON_ERROR(SetRegisterBit(kRegisterCtrl1, 2), kTag,
                                "failed to map FIFO to INT1");
            break;
        case IntPin::kInt2:
            ESP_RETURN_ON_ERROR(ClearRegisterBit(kRegisterCtrl1, 2), kTag,
                                "failed to map FIFO to INT2");
            break;
        case IntPin::kDisabled:
            break;
    }

    fifo_mode_value_ = static_cast<uint8_t>((static_cast<uint8_t>(samples) << 2) |
                                            static_cast<uint8_t>(mode));
    config_.enable_fifo = mode != FifoMode::kBypass;
    config_.fifo_mode = mode;
    config_.fifo_samples = samples;
    config_.fifo_watermark = trigger_samples;

    ESP_RETURN_ON_ERROR(WriteReg(kRegisterFifoCtrl, fifo_mode_value_), kTag,
                        "failed to write FIFO control");
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterFifoWatermark, trigger_samples), kTag,
                        "failed to write FIFO watermark");

    if (gyro_was_enabled) {
        EnableGyroscope();
    }
    if (accel_was_enabled) {
        EnableAccelerometer();
    }

    ESP_LOGI(kTag, "FIFO configured: mode=%u watermark=%u frame_bytes=%u",
             static_cast<unsigned>(mode), static_cast<unsigned>(trigger_samples),
             static_cast<unsigned>(ResolveFifoFrameBytes()));
    return ESP_OK;
}

bool Qmi8658::EnableAccelerometer() {
    if (SetRegisterBit(kRegisterCtrl7, 0) == ESP_OK) {
        accelerometer_enabled_ = true;
        config_.enable_accelerometer = true;
        return true;
    }
    return false;
}

bool Qmi8658::DisableAccelerometer() {
    if (ClearRegisterBit(kRegisterCtrl7, 0) == ESP_OK) {
        accelerometer_enabled_ = false;
        config_.enable_accelerometer = false;
        return true;
    }
    return false;
}

bool Qmi8658::EnableGyroscope() {
    if (SetRegisterBit(kRegisterCtrl7, 1) == ESP_OK) {
        gyroscope_enabled_ = true;
        config_.enable_gyroscope = true;
        return true;
    }
    return false;
}

bool Qmi8658::DisableGyroscope() {
    if (ClearRegisterBit(kRegisterCtrl7, 1) == ESP_OK) {
        gyroscope_enabled_ = false;
        config_.enable_gyroscope = false;
        return true;
    }
    return false;
}

bool Qmi8658::AccelerometerEnabled() const {
    return accelerometer_enabled_;
}

bool Qmi8658::GyroscopeEnabled() const {
    return gyroscope_enabled_;
}

esp_err_t Qmi8658::EnableSyncSampleMode() {
    sample_mode_ = SampleMode::kSync;
    config_.sample_mode = sample_mode_;
    return SetRegisterBit(kRegisterCtrl7, 7);
}

esp_err_t Qmi8658::DisableSyncSampleMode() {
    sample_mode_ = SampleMode::kAsync;
    config_.sample_mode = sample_mode_;
    return ClearRegisterBit(kRegisterCtrl7, 7);
}

esp_err_t Qmi8658::EnableLockingMechanism() {
    ESP_RETURN_ON_ERROR(EnableSyncSampleMode(), kTag, "failed to enable sync sample mode");
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCal1L, 0x01), kTag, "failed to write CAL1_L");
    return WriteCommand(Command::kAhbClockGating, kDefaultCtrl9TimeoutMs, true);
}

esp_err_t Qmi8658::DisableLockingMechanism() {
    ESP_RETURN_ON_ERROR(DisableSyncSampleMode(), kTag, "failed to disable sync sample mode");
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCal1L, 0x00), kTag, "failed to write CAL1_L");
    return WriteCommand(Command::kAhbClockGating, kDefaultCtrl9TimeoutMs, true);
}

void Qmi8658::PowerDown() {
    DisableAccelerometer();
    DisableGyroscope();
    SetRegisterBit(kRegisterCtrl1, 1);
}

void Qmi8658::PowerOn() {
    ClearRegisterBit(kRegisterCtrl1, 1);
}

esp_err_t Qmi8658::ConfigActivityInterruptMap(IntPin pin) {
    return pin == IntPin::kInt1 ? SetRegisterBit(kRegisterCtrl8, 6)
                                : ClearRegisterBit(kRegisterCtrl8, 6);
}

esp_err_t Qmi8658::ConfigPedometer(uint16_t ped_sample_cnt, uint16_t ped_fix_peak2peak,
                                   uint16_t ped_fix_peak, uint16_t ped_time_up,
                                   uint8_t ped_time_low, uint8_t ped_time_cnt_entry,
                                   uint8_t ped_fix_precision, uint8_t ped_sig_count) {
    DisableSyncSampleMode();

    bool gyro_was_enabled = GyroscopeEnabled();
    bool accel_was_enabled = AccelerometerEnabled();
    if (gyro_was_enabled) {
        DisableGyroscope();
    }
    if (accel_was_enabled) {
        DisableAccelerometer();
    }

    WriteReg(kRegisterCal1L, LowByte(ped_sample_cnt));
    WriteReg(kRegisterCal1H, HighByte(ped_sample_cnt));
    WriteReg(kRegisterCal2L, LowByte(ped_fix_peak2peak));
    WriteReg(kRegisterCal2H, HighByte(ped_fix_peak2peak));
    WriteReg(kRegisterCal3L, LowByte(ped_fix_peak));
    WriteReg(kRegisterCal3H, HighByte(ped_fix_peak));
    WriteReg(kRegisterCal4H, 0x01);
    WriteReg(kRegisterCal4L, 0x02);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigurePedometer, kDefaultCtrl9TimeoutMs, true),
                        kTag, "failed to configure pedometer phase 1");

    WriteReg(kRegisterCal1L, LowByte(ped_time_up));
    WriteReg(kRegisterCal1H, HighByte(ped_time_up));
    WriteReg(kRegisterCal2L, ped_time_low);
    WriteReg(kRegisterCal2H, ped_time_cnt_entry);
    WriteReg(kRegisterCal3L, ped_fix_precision);
    WriteReg(kRegisterCal3H, ped_sig_count);
    WriteReg(kRegisterCal4H, 0x02);
    WriteReg(kRegisterCal4L, 0x02);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigurePedometer, kDefaultCtrl9TimeoutMs, true),
                        kTag, "failed to configure pedometer phase 2");

    if (gyro_was_enabled) {
        EnableGyroscope();
    }
    if (accel_was_enabled) {
        EnableAccelerometer();
    }
    return ESP_OK;
}

uint32_t Qmi8658::GetPedometerCounter() {
    uint8_t buffer[3] = {};
    if (ReadRegs(kRegisterStepCount, buffer, sizeof(buffer)) != ESP_OK) {
        return 0;
    }
    return static_cast<uint32_t>(buffer[0]) | (static_cast<uint32_t>(buffer[1]) << 8) |
           (static_cast<uint32_t>(buffer[2]) << 16);
}

esp_err_t Qmi8658::ClearPedometerCounter() {
    return WriteCommand(Command::kResetPedometer, kDefaultCtrl9TimeoutMs, true);
}

bool Qmi8658::EnablePedometer(IntPin pin) {
    if (!AccelerometerEnabled()) {
        return false;
    }
    if (pin == IntPin::kInt1 || pin == IntPin::kInt2) {
        ConfigActivityInterruptMap(pin);
        EnableInt(pin, true);
    }
    return SetRegisterBit(kRegisterCtrl8, 4) == ESP_OK;
}

bool Qmi8658::DisablePedometer() {
    return AccelerometerEnabled() && ClearRegisterBit(kRegisterCtrl8, 4) == ESP_OK;
}

esp_err_t Qmi8658::ConfigTap(uint8_t priority, uint8_t peak_window, uint16_t tap_window,
                             uint16_t double_tap_window, float alpha, float gamma,
                             float peak_mag_threshold_g2, float undefined_motion_threshold_g2) {
    DisableSyncSampleMode();

    bool gyro_was_enabled = GyroscopeEnabled();
    bool accel_was_enabled = AccelerometerEnabled();
    if (gyro_was_enabled) {
        DisableGyroscope();
    }
    if (accel_was_enabled) {
        DisableAccelerometer();
    }

    WriteReg(kRegisterCal1L, peak_window);
    WriteReg(kRegisterCal1H, priority);
    WriteReg(kRegisterCal2L, LowByte(tap_window));
    WriteReg(kRegisterCal2H, HighByte(tap_window));
    WriteReg(kRegisterCal3L, LowByte(double_tap_window));
    WriteReg(kRegisterCal3H, HighByte(double_tap_window));
    WriteReg(kRegisterCal4H, 0x01);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigureTap, kDefaultCtrl9TimeoutMs, true), kTag,
                        "failed to configure tap phase 1");

    WriteReg(kRegisterCal1L, static_cast<uint8_t>(alpha * 128.0f));
    WriteReg(kRegisterCal1H, static_cast<uint8_t>(gamma * 128.0f));

    constexpr double kGravity = 9.81;
    constexpr double kResolution = 0.001 * kGravity * kGravity;
    uint16_t peak_value = static_cast<uint16_t>((peak_mag_threshold_g2 * kGravity * kGravity) /
                                                kResolution);
    uint16_t quiet_value =
        static_cast<uint16_t>((undefined_motion_threshold_g2 * kGravity * kGravity) /
                              kResolution);
    WriteReg(kRegisterCal2L, LowByte(peak_value));
    WriteReg(kRegisterCal2H, HighByte(peak_value));
    WriteReg(kRegisterCal3L, LowByte(quiet_value));
    WriteReg(kRegisterCal3H, HighByte(quiet_value));
    WriteReg(kRegisterCal4H, 0x02);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigureTap, kDefaultCtrl9TimeoutMs, true), kTag,
                        "failed to configure tap phase 2");

    if (gyro_was_enabled) {
        EnableGyroscope();
    }
    if (accel_was_enabled) {
        EnableAccelerometer();
    }
    return ESP_OK;
}

bool Qmi8658::EnableTap(IntPin pin) {
    if (!AccelerometerEnabled()) {
        return false;
    }
    if (pin == IntPin::kInt1 || pin == IntPin::kInt2) {
        ConfigActivityInterruptMap(pin);
        EnableInt(pin, true);
    }
    return SetRegisterBit(kRegisterCtrl8, 0) == ESP_OK;
}

bool Qmi8658::DisableTap() {
    return ClearRegisterBit(kRegisterCtrl8, 0) == ESP_OK;
}

Qmi8658::TapEvent Qmi8658::GetTapStatus(uint8_t* raw_status) {
    uint8_t value = 0;
    if (ReadReg(kRegisterTapStatus, &value) != ESP_OK) {
        return TapEvent::kInvalid;
    }
    if (raw_status != nullptr) {
        *raw_status = value;
    }
    switch (value & 0x03) {
        case 1:
            return TapEvent::kSingle;
        case 2:
            return TapEvent::kDouble;
        default:
            return TapEvent::kInvalid;
    }
}

esp_err_t Qmi8658::ConfigMotion(uint8_t mode_ctrl, float any_motion_x_threshold_mg,
                                float any_motion_y_threshold_mg, float any_motion_z_threshold_mg,
                                uint8_t any_motion_window, float no_motion_x_threshold_mg,
                                float no_motion_y_threshold_mg, float no_motion_z_threshold_mg,
                                uint8_t no_motion_window,
                                uint16_t significant_motion_wait_window,
                                uint16_t significant_motion_confirm_window) {
    DisableSyncSampleMode();

    bool gyro_was_enabled = GyroscopeEnabled();
    bool accel_was_enabled = AccelerometerEnabled();
    if (gyro_was_enabled) {
        DisableGyroscope();
    }
    if (accel_was_enabled) {
        DisableAccelerometer();
    }

    WriteReg(kRegisterCal1L, MgToBytes(any_motion_x_threshold_mg));
    WriteReg(kRegisterCal1H, MgToBytes(any_motion_y_threshold_mg));
    WriteReg(kRegisterCal2L, MgToBytes(any_motion_z_threshold_mg));
    WriteReg(kRegisterCal2H, MgToBytes(no_motion_x_threshold_mg));
    WriteReg(kRegisterCal3L, MgToBytes(no_motion_y_threshold_mg));
    WriteReg(kRegisterCal3H, MgToBytes(no_motion_z_threshold_mg));
    WriteReg(kRegisterCal4L, mode_ctrl);
    WriteReg(kRegisterCal4H, 0x01);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigureMotion, kDefaultCtrl9TimeoutMs, true),
                        kTag, "failed to configure motion phase 1");

    WriteReg(kRegisterCal1L, any_motion_window);
    WriteReg(kRegisterCal1H, no_motion_window);
    WriteReg(kRegisterCal2L, LowByte(significant_motion_wait_window));
    WriteReg(kRegisterCal2H, HighByte(significant_motion_wait_window));
    WriteReg(kRegisterCal3L, LowByte(significant_motion_confirm_window));
    WriteReg(kRegisterCal3H, HighByte(significant_motion_confirm_window));
    WriteReg(kRegisterCal4H, 0x02);
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kConfigureMotion, kDefaultCtrl9TimeoutMs, true),
                        kTag, "failed to configure motion phase 2");

    if (gyro_was_enabled) {
        EnableGyroscope();
    }
    if (accel_was_enabled) {
        EnableAccelerometer();
    }
    return ESP_OK;
}

bool Qmi8658::EnableMotionDetect(IntPin pin) {
    if (!AccelerometerEnabled()) {
        return false;
    }
    if (pin == IntPin::kInt1 || pin == IntPin::kInt2) {
        ConfigActivityInterruptMap(pin);
        EnableInt(pin, true);
    }
    return SetRegisterBit(kRegisterCtrl8, 1) == ESP_OK &&
           SetRegisterBit(kRegisterCtrl8, 2) == ESP_OK &&
           SetRegisterBit(kRegisterCtrl8, 3) == ESP_OK;
}

bool Qmi8658::DisableMotionDetect() {
    return ClearRegisterBit(kRegisterCtrl8, 1) == ESP_OK &&
           ClearRegisterBit(kRegisterCtrl8, 2) == ESP_OK &&
           ClearRegisterBit(kRegisterCtrl8, 3) == ESP_OK;
}

esp_err_t Qmi8658::ConfigWakeOnMotion(uint8_t wom_threshold_mg, AccOdr odr, IntPin pin,
                                      uint8_t default_pin_value, uint8_t blanking_time) {
    ESP_RETURN_ON_ERROR(Reset(true, 500), kTag, "failed to reset IMU for wake on motion");
    ESP_RETURN_ON_ERROR(ClearRegisterBit(kRegisterCtrl7, 0), kTag,
                        "failed to disable accelerometer before WoM");
    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl2, 0x8F,
                                           static_cast<uint8_t>(static_cast<uint8_t>(AccRange::k8g)
                                                                << 4)),
                        kTag, "failed to set WoM accelerometer range");
    ESP_RETURN_ON_ERROR(ReadModifyRegister(kRegisterCtrl2, 0xF0, static_cast<uint8_t>(odr)), kTag,
                        "failed to set WoM ODR");
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCal1L, wom_threshold_mg), kTag,
                        "failed to set WoM threshold");

    uint8_t route_value = 0;
    if (pin == IntPin::kInt1) {
        route_value = default_pin_value ? 0x02 : 0x00;
    } else {
        route_value = default_pin_value ? 0x03 : 0x01;
    }
    route_value <<= 6;
    route_value |= (blanking_time & 0x3F);
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCal1H, route_value), kTag,
                        "failed to set WoM route");
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kWriteWomSetting, kDefaultCtrl9TimeoutMs, true),
                        kTag, "failed to apply WoM settings");

    EnableAccelerometer();
    return EnableInt(pin, true);
}

uint16_t Qmi8658::Update() {
    uint8_t status[3] = {};
    if (ReadRegs(kRegisterStatusInt, status, sizeof(status)) != ESP_OK) {
        return 0;
    }
    return DecodeStatus(status[0], status[1], status[2], true);
}

void Qmi8658::SetWakeOnMotionCallback(EventCallback callback) {
    wake_on_motion_callback_ = std::move(callback);
}

void Qmi8658::SetTapCallback(EventCallback callback) {
    tap_callback_ = std::move(callback);
}

void Qmi8658::SetPedometerCallback(EventCallback callback) {
    pedometer_callback_ = std::move(callback);
}

void Qmi8658::SetNoMotionCallback(EventCallback callback) {
    no_motion_callback_ = std::move(callback);
}

void Qmi8658::SetAnyMotionCallback(EventCallback callback) {
    any_motion_callback_ = std::move(callback);
}

void Qmi8658::SetSignificantMotionCallback(EventCallback callback) {
    significant_motion_callback_ = std::move(callback);
}

void Qmi8658::SetGyroDataReadyCallback(EventCallback callback) {
    gyro_data_ready_callback_ = std::move(callback);
}

void Qmi8658::SetAccelDataReadyCallback(EventCallback callback) {
    accel_data_ready_callback_ = std::move(callback);
}

void Qmi8658::SetDataLockingCallback(EventCallback callback) {
    data_locking_callback_ = std::move(callback);
}

void IRAM_ATTR Qmi8658::Interrupt2IsrHandler(void* arg) {
    auto* imu = static_cast<Qmi8658*>(arg);
    if (imu == nullptr || imu->interrupt2_isr_queue_ == nullptr) {
        return;
    }

    // Avoid calling non-IRAM-safe GPIO helpers while flash cache may be disabled.
    uint32_t level = 1;
    BaseType_t higher_priority_task_woken = pdFALSE;
    xQueueSendFromISR(imu->interrupt2_isr_queue_, &level, &higher_priority_task_woken);
    if (higher_priority_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

void Qmi8658::InterruptTaskEntry(void* arg) {
    auto* imu = static_cast<Qmi8658*>(arg);
    if (imu != nullptr) {
        imu->InterruptTask();
    }
    vTaskDelete(nullptr);
}

void Qmi8658::InterruptTask() {
    while (true) {
        uint32_t level = 0;
        if (xQueueReceive(interrupt2_isr_queue_, &level, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        InterruptEvent event = {
            .level = config_.interrupt2_pin != GPIO_NUM_NC ? gpio_get_level(config_.interrupt2_pin)
                                                           : static_cast<int>(level),
            .status_int = 0,
            .status0 = 0,
            .status1 = 0,
            .sensor_status = 0,
        };

        if (ReadStatusInt(&event.status_int) != ESP_OK) {
            ESP_LOGW(kTag, "failed to read QMI8658 statusint after INT2 event");
        }
        if (ReadStatus0(&event.status0) != ESP_OK) {
            ESP_LOGW(kTag, "failed to read QMI8658 status0 after INT2 event");
        }
        if (ReadStatus1(&event.status1) != ESP_OK) {
            ESP_LOGW(kTag, "failed to read QMI8658 status1 after INT2 event");
        }

        size_t bytes_read = 0;
        if (config_.enable_fifo) {
            esp_err_t err = ReadFromFifo(&bytes_read);
            if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
                ESP_LOGW(kTag, "failed to drain FIFO after INT2 event: %s", esp_err_to_name(err));
            }
        }

        event.sensor_status = DecodeStatus(event.status_int, event.status0, event.status1, true);
        bool has_motion_status =
            (event.status1 & static_cast<uint8_t>(StatusEvent::kSignificantMotion)) != 0 ||
            (event.status1 & static_cast<uint8_t>(StatusEvent::kNoMotion)) != 0 ||
            (event.status1 & static_cast<uint8_t>(StatusEvent::kAnyMotion)) != 0;
        if (has_motion_status) {
            ESP_LOGD(kTag,
                     "INT2 event: level=%d statusint=0x%02X status0=0x%02X status1=0x%02X bytes=%u",
                     event.level, event.status_int, event.status0, event.status1,
                     static_cast<unsigned>(bytes_read));
        } else {
            ESP_LOGV(kTag,
                     "INT2 edge without motion status: level=%d statusint=0x%02X status0=0x%02X "
                     "status1=0x%02X bytes=%u",
                     event.level, event.status_int, event.status0, event.status1,
                     static_cast<unsigned>(bytes_read));
        }
        if (interrupt2_event_queue_ != nullptr) {
            xQueueSend(interrupt2_event_queue_, &event, 0);
        }
        if (interrupt2_callback_) {
            interrupt2_callback_(event);
        }
    }
}

esp_err_t Qmi8658::WriteCommand(Command command, uint32_t wait_ms, bool log_failures) {
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCtrl9, static_cast<uint8_t>(command)), kTag,
                        "failed to write CTRL9");

    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(wait_ms);
    while (xTaskGetTickCount() < deadline) {
        uint8_t status_int = 0;
        ESP_RETURN_ON_ERROR(ReadStatusInt(&status_int), kTag, "failed to poll STATUS_INT");
        if ((status_int & kStatusIntCtrl9Done) != 0) {
            ESP_RETURN_ON_ERROR(WriteReg(kRegisterCtrl9, static_cast<uint8_t>(Command::kAck)),
                                kTag, "failed to ACK CTRL9 command");
            while (xTaskGetTickCount() < deadline) {
                ESP_RETURN_ON_ERROR(ReadStatusInt(&status_int), kTag,
                                    "failed to poll STATUS_INT clear");
                if ((status_int & kStatusIntCtrl9Done) == 0) {
                    return ESP_OK;
                }
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            if (log_failures) {
                ESP_LOGW(kTag, "QMI8658 CTRL9 ACK for command 0x%02X timed out",
                         static_cast<unsigned>(command));
            }
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (log_failures) {
        ESP_LOGW(kTag, "QMI8658 CTRL9 command 0x%02X timed out", static_cast<unsigned>(command));
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t Qmi8658::ReadModifyRegister(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = 0;
    ESP_RETURN_ON_ERROR(ReadReg(reg, &current), kTag, "failed to read register");
    current &= mask;
    current |= value;
    return WriteReg(reg, current);
}

esp_err_t Qmi8658::SetRegisterBit(uint8_t reg, uint8_t bit_index) {
    uint8_t current = 0;
    ESP_RETURN_ON_ERROR(ReadReg(reg, &current), kTag, "failed to read register");
    current |= static_cast<uint8_t>(1U << bit_index);
    return WriteReg(reg, current);
}

esp_err_t Qmi8658::ClearRegisterBit(uint8_t reg, uint8_t bit_index) {
    uint8_t current = 0;
    ESP_RETURN_ON_ERROR(ReadReg(reg, &current), kTag, "failed to read register");
    current &= static_cast<uint8_t>(~(1U << bit_index));
    return WriteReg(reg, current);
}

esp_err_t Qmi8658::ConfigureInterruptPins() {
    uint64_t mask = 0;
    if (config_.interrupt1_pin != GPIO_NUM_NC) {
        mask |= 1ULL << config_.interrupt1_pin;
    }
    if (config_.interrupt2_pin != GPIO_NUM_NC) {
        mask |= 1ULL << config_.interrupt2_pin;
    }
    if (mask == 0) {
        return ESP_OK;
    }

    gpio_config_t gpio_config_data = {};
    gpio_config_data.intr_type =
        config_.interrupt2_pin != GPIO_NUM_NC ? GPIO_INTR_POSEDGE : GPIO_INTR_DISABLE;
    gpio_config_data.mode = GPIO_MODE_INPUT;
    gpio_config_data.pin_bit_mask = mask;
    gpio_config_data.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config_data.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&gpio_config_data), kTag,
                        "failed to configure IMU interrupt pins");

    if (config_.interrupt2_pin != GPIO_NUM_NC) {
        // Install the shared GPIO ISR service; ESP_ERR_INVALID_STATE means another
        // driver already installed it, which is fine.
        const esp_err_t isr_err = gpio_install_isr_service(0);
        if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(kTag, "failed to install GPIO ISR service: %s", esp_err_to_name(isr_err));
            return isr_err;
        }
        ESP_RETURN_ON_ERROR(gpio_isr_handler_add(config_.interrupt2_pin, Interrupt2IsrHandler, this),
                            kTag, "failed to add IMU INT2 ISR handler");
    }

    return ESP_OK;
}

esp_err_t Qmi8658::ConfigureDefaultState() {
    ESP_RETURN_ON_ERROR(WriteReg(kRegisterCtrl1, kCtrl1AddrAutoIncrement), kTag,
                        "failed to write CTRL1");
    return WriteReg(kRegisterCtrl8, kCtrl8StatusIntCtrl9Handshake);
}

esp_err_t Qmi8658::CopyUsidAndRevision() {
    ESP_RETURN_ON_ERROR(WriteCommand(Command::kCopyUsid, kDefaultCtrl9TimeoutMs, true), kTag,
                        "failed to issue COPY_USID");

    uint8_t buffer[3] = {};
    if (ReadRegs(kRegisterDqw, buffer, sizeof(buffer)) == ESP_OK) {
        revision_id_ = static_cast<uint32_t>(buffer[0]) | (static_cast<uint32_t>(buffer[1]) << 8) |
                       (static_cast<uint32_t>(buffer[2]) << 16);
    }
    ReadRegs(kRegisterDvx, usid_, sizeof(usid_));
    return ESP_OK;
}

esp_err_t Qmi8658::SyncLatestSampleFromRegisters() {
    Qmi8658Sample sample = {};
    sample.timestamp = GetTimestamp();

    int16_t raw_acc[3] = {};
    if (accelerometer_enabled_) {
        ESP_RETURN_ON_ERROR(ReadRawAxes(kRegisterAcceleration, raw_acc), kTag,
                            "failed to read acceleration");
        for (size_t i = 0; i < 3; ++i) {
            sample.acceleration_mg[i] = ConvertAccToMg(raw_acc[i], acc_lsb_div_);
        }
    }

    int16_t raw_gyro[3] = {};
    if (gyroscope_enabled_) {
        ESP_RETURN_ON_ERROR(ReadRawAxes(kRegisterGyroscope, raw_gyro), kTag,
                            "failed to read gyroscope");
        for (size_t i = 0; i < 3; ++i) {
            sample.gyro_dps[i] = ConvertGyroToDps(raw_gyro[i], gyro_lsb_div_);
        }
    }

    float temperature_c = 0.0f;
    bool have_temperature = ReadTemperatureRegister(&temperature_c) == ESP_OK;
    std::lock_guard<std::mutex> lock(sample_mutex_);
    latest_sample_ = sample;
    latest_sample_valid_ = true;
    if (have_temperature) {
        latest_temperature_c_ = temperature_c;
        latest_temperature_valid_ = true;
    }
    return ESP_OK;
}

esp_err_t Qmi8658::DecodeFifoBuffer(size_t fifo_bytes, bool* updated_sample) {
    if (updated_sample != nullptr) {
        *updated_sample = false;
    }
    if (fifo_bytes == 0 || fifo_buffer_.size() < fifo_bytes) {
        return ESP_ERR_INVALID_SIZE;
    }

    Qmi8658Sample sample = {};
    const bool dual_sensor = accelerometer_enabled_ && gyroscope_enabled_;
    const size_t total_blocks = fifo_bytes / 6;
    auto publish_sample = [this](const Qmi8658Sample& fifo_sample) {
        if (fifo_sample_callback_) {
            Qmi8658Sample published = fifo_sample;
            published.timestamp = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
            fifo_sample_callback_(published);
        }
    };

    for (size_t block = 0; block < total_blocks; ++block) {
        const uint8_t* raw = fifo_buffer_.data() + (block * 6);
        int16_t x = static_cast<int16_t>((raw[1] << 8) | raw[0]);
        int16_t y = static_cast<int16_t>((raw[3] << 8) | raw[2]);
        int16_t z = static_cast<int16_t>((raw[5] << 8) | raw[4]);

        if (dual_sensor) {
            if ((block % 2) == 0) {
                sample.acceleration_mg[0] = ConvertAccToMg(x, acc_lsb_div_);
                sample.acceleration_mg[1] = ConvertAccToMg(y, acc_lsb_div_);
                sample.acceleration_mg[2] = ConvertAccToMg(z, acc_lsb_div_);
            } else {
                sample.gyro_dps[0] = ConvertGyroToDps(x, gyro_lsb_div_);
                sample.gyro_dps[1] = ConvertGyroToDps(y, gyro_lsb_div_);
                sample.gyro_dps[2] = ConvertGyroToDps(z, gyro_lsb_div_);
                publish_sample(sample);
            }
        } else if (accelerometer_enabled_) {
            sample.acceleration_mg[0] = ConvertAccToMg(x, acc_lsb_div_);
            sample.acceleration_mg[1] = ConvertAccToMg(y, acc_lsb_div_);
            sample.acceleration_mg[2] = ConvertAccToMg(z, acc_lsb_div_);
            publish_sample(sample);
        } else if (gyroscope_enabled_) {
            sample.gyro_dps[0] = ConvertGyroToDps(x, gyro_lsb_div_);
            sample.gyro_dps[1] = ConvertGyroToDps(y, gyro_lsb_div_);
            sample.gyro_dps[2] = ConvertGyroToDps(z, gyro_lsb_div_);
            publish_sample(sample);
        }
    }

    sample.timestamp = static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);

    float temperature_c = 0.0f;
    bool have_temperature = ReadTemperatureRegister(&temperature_c) == ESP_OK;
    {
        std::lock_guard<std::mutex> lock(sample_mutex_);
        latest_sample_ = sample;
        latest_sample_valid_ = true;
        if (have_temperature) {
            latest_temperature_c_ = temperature_c;
            latest_temperature_valid_ = true;
        }
    }

    if (updated_sample != nullptr) {
        *updated_sample = true;
    }
    return ESP_OK;
}

esp_err_t Qmi8658::ReadTemperatureRegister(float* temperature_c) {
    if (temperature_c == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buffer[2] = {};
    ESP_RETURN_ON_ERROR(ReadRegs(kRegisterTemperature, buffer, sizeof(buffer)), kTag,
                        "failed to read IMU temperature");
    int16_t raw = static_cast<int16_t>((buffer[1] << 8) | buffer[0]);
    *temperature_c = static_cast<float>(raw) / 256.0f;
    return ESP_OK;
}

esp_err_t Qmi8658::ReadRawAxes(uint8_t start_reg, int16_t xyz[3]) {
    if (xyz == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t buffer[6] = {};
    ESP_RETURN_ON_ERROR(ReadRegs(start_reg, buffer, sizeof(buffer)), kTag, "failed to read IMU axes");
    for (size_t i = 0; i < 3; ++i) {
        xyz[i] = static_cast<int16_t>((buffer[(i * 2) + 1] << 8) | buffer[i * 2]);
    }
    return ESP_OK;
}

uint16_t Qmi8658::ResolveAccLsbDiv() const {
    switch (config_.acc_range) {
        case AccRange::k2g:
            return 1 << 14;
        case AccRange::k4g:
            return 1 << 13;
        case AccRange::k8g:
            return 1 << 12;
        case AccRange::k16g:
            return 1 << 11;
    }
    return 1 << 12;
}

uint16_t Qmi8658::ResolveGyroLsbDiv() const {
    switch (config_.gyro_range) {
        case GyroRange::k16Dps:
            return 2048;
        case GyroRange::k32Dps:
            return 1024;
        case GyroRange::k64Dps:
            return 512;
        case GyroRange::k128Dps:
            return 256;
        case GyroRange::k256Dps:
            return 128;
        case GyroRange::k512Dps:
            return 64;
        case GyroRange::k1024Dps:
            return 32;
    }
    return 64;
}

uint8_t Qmi8658::ResolveCtrl7EnableMask() const {
    uint8_t value = 0;
    if (config_.enable_accelerometer) {
        value |= kCtrl7AccelEnable;
    }
    if (config_.enable_gyroscope) {
        value |= kCtrl7GyroEnable;
    }
    return value;
}

size_t Qmi8658::ResolveFifoFrameBytes() const {
    size_t bytes = 0;
    if (accelerometer_enabled_) {
        bytes += 6;
    }
    if (gyroscope_enabled_) {
        bytes += 6;
    }
    return bytes;
}

uint16_t Qmi8658::ResolveFifoCapacityBytes() const {
    static constexpr uint8_t kSampleCounts[] = {16, 32, 64, 128};
    const uint8_t sensor_count = (accelerometer_enabled_ && gyroscope_enabled_)
                                     ? 2
                                     : ((accelerometer_enabled_ || gyroscope_enabled_) ? 1 : 0);
    return static_cast<uint16_t>(kSampleCounts[static_cast<uint8_t>(config_.fifo_samples)] * 6 *
                                 sensor_count);
}

uint16_t Qmi8658::DecodeStatus(uint8_t status_int, uint8_t status0, uint8_t status1,
                               bool dispatch_callbacks) {
    SensorStatus result = SensorStatus::kNone;
    constexpr uint8_t kMotionStatusMask =
        static_cast<uint8_t>(StatusEvent::kSignificantMotion) |
        static_cast<uint8_t>(StatusEvent::kNoMotion) |
        static_cast<uint8_t>(StatusEvent::kAnyMotion);
    const uint8_t previous_motion_status = last_motion_status1_ & kMotionStatusMask;
    uint8_t next_motion_status = previous_motion_status;

    if ((status_int & kStatusIntCtrl9Done) != 0) {
        result |= SensorStatus::kCtrl9CommandDone;
    }
    if ((status_int & kStatusIntLocked) != 0) {
        result |= SensorStatus::kLocked;
    }
    if ((status_int & kStatusIntAvailable) != 0) {
        result |= SensorStatus::kAvailable;
    }
    if ((status_int & (kStatusIntLocked | kStatusIntAvailable)) ==
            (kStatusIntLocked | kStatusIntAvailable) &&
        dispatch_callbacks && data_locking_callback_) {
        data_locking_callback_();
    }

    if (sample_mode_ == SampleMode::kAsync) {
        if ((status0 & kStatus0GyroReady) != 0) {
            result |= SensorStatus::kGyroDataReady;
            gyroscope_data_ready_ = true;
            if (dispatch_callbacks && gyro_data_ready_callback_) {
                gyro_data_ready_callback_();
            }
        }
        if ((status0 & kStatus0AccelReady) != 0) {
            result |= SensorStatus::kAccelDataReady;
            acceleration_data_ready_ = true;
            if (dispatch_callbacks && accel_data_ready_callback_) {
                accel_data_ready_callback_();
            }
        }
    }

    if ((status1 & static_cast<uint8_t>(StatusEvent::kSignificantMotion)) != 0) {
        result |= SensorStatus::kSignificantMotion;
        if (dispatch_callbacks && significant_motion_callback_ &&
            (previous_motion_status &
             static_cast<uint8_t>(StatusEvent::kSignificantMotion)) == 0) {
            significant_motion_callback_();
        }
        next_motion_status |= static_cast<uint8_t>(StatusEvent::kSignificantMotion);
    }
    if ((status1 & static_cast<uint8_t>(StatusEvent::kNoMotion)) != 0) {
        result |= SensorStatus::kNoMotion;
        if (dispatch_callbacks && no_motion_callback_ &&
            (previous_motion_status & static_cast<uint8_t>(StatusEvent::kNoMotion)) == 0) {
            no_motion_callback_();
        }
        next_motion_status |= static_cast<uint8_t>(StatusEvent::kNoMotion);
        next_motion_status &= ~static_cast<uint8_t>(StatusEvent::kAnyMotion);
    }
    if ((status1 & static_cast<uint8_t>(StatusEvent::kAnyMotion)) != 0) {
        result |= SensorStatus::kAnyMotion;
        if (dispatch_callbacks && any_motion_callback_ &&
            (previous_motion_status & static_cast<uint8_t>(StatusEvent::kAnyMotion)) == 0) {
            any_motion_callback_();
        }
        next_motion_status |= static_cast<uint8_t>(StatusEvent::kAnyMotion);
        next_motion_status &= ~static_cast<uint8_t>(StatusEvent::kNoMotion);
    }
    if ((status1 & static_cast<uint8_t>(StatusEvent::kPedometerMotion)) != 0) {
        result |= SensorStatus::kPedometerMotion;
        if (dispatch_callbacks && pedometer_callback_) {
            pedometer_callback_();
        }
    }
    if ((status1 & static_cast<uint8_t>(StatusEvent::kWakeOnMotion)) != 0) {
        result |= SensorStatus::kWakeOnMotion;
        if (dispatch_callbacks && wake_on_motion_callback_) {
            wake_on_motion_callback_();
        }
    }
    if ((status1 & static_cast<uint8_t>(StatusEvent::kTap)) != 0) {
        result |= SensorStatus::kTap;
        if (dispatch_callbacks && tap_callback_) {
            tap_callback_();
        }
    }

    last_motion_status1_ = (last_motion_status1_ & ~kMotionStatusMask) | next_motion_status;

    return static_cast<uint16_t>(result);
}

uint8_t Qmi8658::MgToBytes(float mg) const {
    float grams = mg / 1000.0f;
    int units = static_cast<int>(std::round(grams / 0.03125f));
    return static_cast<uint8_t>((units & 0x1F) << 3);
}

float Qmi8658::ConvertAccToMg(int16_t raw, uint16_t divisor) {
    return static_cast<float>(raw) * 1000.0f / static_cast<float>(divisor);
}

float Qmi8658::ConvertGyroToDps(int16_t raw, uint16_t divisor) {
    return static_cast<float>(raw) / static_cast<float>(divisor);
}
