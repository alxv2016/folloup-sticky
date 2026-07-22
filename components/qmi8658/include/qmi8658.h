#ifndef QMI8658_H
#define QMI8658_H

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>

#include <functional>
#include <mutex>
#include <vector>

#include "i2c_device.h"

struct Qmi8658Sample {
    float acceleration_mg[3] = {};
    float gyro_dps[3] = {};
    uint32_t timestamp = 0;
};

class Qmi8658 : public I2cDevice {
public:
    enum class AccRange : uint8_t {
        k2g = 0,
        k4g = 1,
        k8g = 2,
        k16g = 3,
    };

    enum class GyroRange : uint8_t {
        k16Dps = 0,
        k32Dps = 1,
        k64Dps = 2,
        k128Dps = 3,
        k256Dps = 4,
        k512Dps = 5,
        k1024Dps = 6,
    };

    enum class AccOdr : uint8_t {
        k1000Hz = 3,
        k500Hz = 4,
        k250Hz = 5,
        k125Hz = 6,
        k62_5Hz = 7,
        k31_25Hz = 8,
        kLowPower128Hz = 12,
        kLowPower21Hz = 13,
        kLowPower11Hz = 14,
        kLowPower3Hz = 15,
    };

    enum class GyroOdr : uint8_t {
        k7174_4Hz = 0,
        k3587_2Hz = 1,
        k1793_6Hz = 2,
        k896_8Hz = 3,
        k448_4Hz = 4,
        k224_2Hz = 5,
        k112_1Hz = 6,
        k56_05Hz = 7,
        k28_025Hz = 8,
        k125Hz = 6,
    };

    enum class LpfMode : uint8_t {
        kMode0 = 0,
        kMode1 = 1,
        kMode2 = 2,
        kMode3 = 3,
        kOff = 4,
    };

    enum class MotionEvent : uint8_t {
        kTap = 0,
        kAnyMotion = 1,
        kNoMotion = 2,
        kSignificantMotion = 3,
        kPedometer = 4,
    };

    enum class IntPin : uint8_t {
        kInt1 = 0,
        kInt2 = 1,
        kDisabled = 2,
    };

    enum class FifoSamples : uint8_t {
        k16 = 0,
        k32 = 1,
        k64 = 2,
        k128 = 3,
    };

    enum class FifoMode : uint8_t {
        kBypass = 0,
        kFifo = 1,
        kStream = 2,
    };

    enum class SampleMode : uint8_t {
        kSync = 0,
        kAsync = 1,
    };

    enum class Command : uint8_t {
        kAck = 0x00,
        kResetFifo = 0x04,
        kRequestFifo = 0x05,
        kWriteWomSetting = 0x08,
        kAccelHostDeltaOffset = 0x09,
        kGyroHostDeltaOffset = 0x0A,
        kConfigureTap = 0x0C,
        kConfigurePedometer = 0x0D,
        kConfigureMotion = 0x0E,
        kResetPedometer = 0x0F,
        kCopyUsid = 0x10,
        kSetRpu = 0x11,
        kAhbClockGating = 0x12,
        kOnDemandCalibration = 0xA2,
        kApplyGyroGains = 0xAA,
    };

    enum class StatusEvent : uint8_t {
        kSignificantMotion = 0x80,
        kNoMotion = 0x40,
        kAnyMotion = 0x20,
        kPedometerMotion = 0x10,
        kWakeOnMotion = 0x04,
        kTap = 0x02,
    };

    enum class SensorStatus : uint16_t {
        kNone = 0,
        kCtrl9CommandDone = 1 << 0,
        kLocked = 1 << 1,
        kAvailable = 1 << 2,
        kGyroDataReady = 1 << 3,
        kAccelDataReady = 1 << 4,
        kSignificantMotion = 1 << 5,
        kNoMotion = 1 << 6,
        kAnyMotion = 1 << 7,
        kPedometerMotion = 1 << 8,
        kWakeOnMotion = 1 << 9,
        kTap = 1 << 10,
    };

    enum class TapDetectionPriority : uint8_t {
        kPriority0 = 0,
        kPriority1 = 1,
        kPriority2 = 2,
        kPriority3 = 3,
        kPriority4 = 4,
        kPriority5 = 5,
    };

    enum class MotionCtrl : uint8_t {
        kAnyMotionEnableX = 1 << 0,
        kAnyMotionEnableY = 1 << 1,
        kAnyMotionEnableZ = 1 << 2,
        kAnyMotionLogicAnd = 1 << 3,
        kNoMotionEnableX = 1 << 4,
        kNoMotionEnableY = 1 << 5,
        kNoMotionEnableZ = 1 << 6,
        kNoMotionLogicOr = 1 << 7,
    };

    enum class TapEvent : uint8_t {
        kInvalid = 0,
        kSingle = 1,
        kDouble = 2,
    };

    struct InterruptEvent {
        int level = -1;
        uint8_t status_int = 0;
        uint8_t status0 = 0;
        uint8_t status1 = 0;
        uint16_t sensor_status = 0;
    };

    using InterruptCallback = std::function<void(const InterruptEvent&)>;
    using EventCallback = std::function<void()>;
    using FifoSampleCallback = std::function<void(const Qmi8658Sample&)>;

    static constexpr uint8_t kDefaultAddress = 0x6B;
    static constexpr uint8_t kAlternateAddress = 0x6A;

    struct Config {
        AccRange acc_range = AccRange::k8g;
        AccOdr acc_odr = AccOdr::k125Hz;
        LpfMode acc_lpf_mode = LpfMode::kMode0;
        GyroRange gyro_range = GyroRange::k512Dps;
        GyroOdr gyro_odr = GyroOdr::k125Hz;
        LpfMode gyro_lpf_mode = LpfMode::kMode0;
        bool enable_accelerometer = true;
        bool enable_gyroscope = true;
        bool enable_data_ready_interrupt = false;
        SampleMode sample_mode = SampleMode::kAsync;
        bool enable_fifo = false;
        FifoMode fifo_mode = FifoMode::kFifo;
        FifoSamples fifo_samples = FifoSamples::k128;
        uint8_t fifo_watermark = 16;
        bool enable_wake_on_motion = false;
        uint8_t wom_threshold_mg = 200;
        AccOdr wom_odr = AccOdr::kLowPower128Hz;
        gpio_num_t interrupt1_pin = GPIO_NUM_NC;
        gpio_num_t interrupt2_pin = GPIO_NUM_NC;
    };

    explicit Qmi8658(i2c_master_bus_handle_t i2c_bus, uint8_t addr = kDefaultAddress);
    Qmi8658(i2c_master_bus_handle_t i2c_bus, uint8_t addr, const Config& config);

    esp_err_t Initialize(bool log_failures = true);
    esp_err_t Reset(bool wait_result = true, uint32_t timeout_ms = 500);
    esp_err_t ReadIdentity(uint8_t* chip_id, uint8_t* revision = nullptr, bool log_failures = true);
    esp_err_t ReadTemperature(float* temperature_c);
    esp_err_t ReadAcceleration(float acceleration_mg[3]);
    esp_err_t ReadGyroscope(float gyro_dps[3]);
    esp_err_t ReadSample(Qmi8658Sample* sample, bool read_timestamp = true);
    esp_err_t ReadLatestSample(Qmi8658Sample* sample, float* temperature_c = nullptr);
    esp_err_t ReadStatusInt(uint8_t* status);
    esp_err_t ReadStatus0(uint8_t* status);
    esp_err_t ReadStatus1(uint8_t* status);
    esp_err_t ReadFromFifo(size_t* bytes_read = nullptr);
    bool GetDataReady();
    uint32_t GetTimestamp();
    int GetInterrupt2Level() const;
    void SetInterrupt2Callback(InterruptCallback callback);
    void SetFifoSampleCallback(FifoSampleCallback callback);

    esp_err_t EnableInt(IntPin pin, bool enable = true);
    esp_err_t EnableDataReadyInterrupt(bool enable = true);
    esp_err_t ConfigAccelerometer(AccRange range, AccOdr odr, LpfMode lpf_mode = LpfMode::kMode0);
    esp_err_t ConfigGyroscope(GyroRange range, GyroOdr odr, LpfMode lpf_mode = LpfMode::kMode0);
    esp_err_t ConfigFifo(FifoMode mode, FifoSamples samples = FifoSamples::k16,
                         IntPin pin = IntPin::kDisabled, uint8_t trigger_samples = 16);
    bool EnableAccelerometer();
    bool DisableAccelerometer();
    bool EnableGyroscope();
    bool DisableGyroscope();
    bool AccelerometerEnabled() const;
    bool GyroscopeEnabled() const;
    esp_err_t EnableSyncSampleMode();
    esp_err_t DisableSyncSampleMode();
    esp_err_t EnableLockingMechanism();
    esp_err_t DisableLockingMechanism();
    void PowerDown();
    void PowerOn();
    esp_err_t ConfigActivityInterruptMap(IntPin pin);
    esp_err_t ConfigPedometer(uint16_t ped_sample_cnt, uint16_t ped_fix_peak2peak,
                              uint16_t ped_fix_peak, uint16_t ped_time_up,
                              uint8_t ped_time_low = 0x14,
                              uint8_t ped_time_cnt_entry = 0x0A,
                              uint8_t ped_fix_precision = 0x00,
                              uint8_t ped_sig_count = 0x04);
    uint32_t GetPedometerCounter();
    esp_err_t ClearPedometerCounter();
    bool EnablePedometer(IntPin pin = IntPin::kDisabled);
    bool DisablePedometer();
    esp_err_t ConfigTap(uint8_t priority, uint8_t peak_window, uint16_t tap_window,
                        uint16_t double_tap_window, float alpha, float gamma,
                        float peak_mag_threshold_g2, float undefined_motion_threshold_g2);
    bool EnableTap(IntPin pin = IntPin::kDisabled);
    bool DisableTap();
    TapEvent GetTapStatus(uint8_t* raw_status = nullptr);
    esp_err_t ConfigMotion(uint8_t mode_ctrl, float any_motion_x_threshold_mg,
                           float any_motion_y_threshold_mg, float any_motion_z_threshold_mg,
                           uint8_t any_motion_window, float no_motion_x_threshold_mg,
                           float no_motion_y_threshold_mg, float no_motion_z_threshold_mg,
                           uint8_t no_motion_window, uint16_t significant_motion_wait_window,
                           uint16_t significant_motion_confirm_window);
    bool EnableMotionDetect(IntPin pin = IntPin::kDisabled);
    bool DisableMotionDetect();
    esp_err_t ConfigWakeOnMotion(uint8_t wom_threshold_mg = 200,
                                 AccOdr odr = AccOdr::kLowPower128Hz,
                                 IntPin pin = IntPin::kInt2,
                                 uint8_t default_pin_value = 1,
                                 uint8_t blanking_time = 0x20);
    uint16_t Update();

    void SetWakeOnMotionCallback(EventCallback callback);
    void SetTapCallback(EventCallback callback);
    void SetPedometerCallback(EventCallback callback);
    void SetNoMotionCallback(EventCallback callback);
    void SetAnyMotionCallback(EventCallback callback);
    void SetSignificantMotionCallback(EventCallback callback);
    void SetGyroDataReadyCallback(EventCallback callback);
    void SetAccelDataReadyCallback(EventCallback callback);
    void SetDataLockingCallback(EventCallback callback);

private:
    static void IRAM_ATTR Interrupt2IsrHandler(void* arg);
    static void InterruptTaskEntry(void* arg);
    void InterruptTask();

    esp_err_t WriteCommand(Command command, uint32_t wait_ms = 1000, bool log_failures = true);
    esp_err_t ReadModifyRegister(uint8_t reg, uint8_t mask, uint8_t value);
    esp_err_t SetRegisterBit(uint8_t reg, uint8_t bit_index);
    esp_err_t ClearRegisterBit(uint8_t reg, uint8_t bit_index);
    esp_err_t ConfigureInterruptPins();
    esp_err_t ConfigureDefaultState();
    esp_err_t CopyUsidAndRevision();
    esp_err_t SyncLatestSampleFromRegisters();
    esp_err_t DecodeFifoBuffer(size_t fifo_bytes, bool* updated_sample);
    esp_err_t ReadTemperatureRegister(float* temperature_c);
    esp_err_t ReadRawAxes(uint8_t start_reg, int16_t xyz[3]);
    uint16_t ResolveAccLsbDiv() const;
    uint16_t ResolveGyroLsbDiv() const;
    uint8_t ResolveCtrl7EnableMask() const;
    size_t ResolveFifoFrameBytes() const;
    uint16_t ResolveFifoCapacityBytes() const;
    uint16_t DecodeStatus(uint8_t status_int, uint8_t status0, uint8_t status1,
                          bool dispatch_callbacks);
    uint8_t MgToBytes(float mg) const;
    static float ConvertAccToMg(int16_t raw, uint16_t divisor);
    static float ConvertGyroToDps(int16_t raw, uint16_t divisor);

    Config config_;
    bool initialized_ = false;
    bool accelerometer_enabled_ = false;
    bool gyroscope_enabled_ = false;
    bool acceleration_data_ready_ = false;
    bool gyroscope_data_ready_ = false;
    bool fifo_interrupt_enabled_ = false;
    IntPin fifo_interrupt_pin_ = IntPin::kDisabled;
    SampleMode sample_mode_ = SampleMode::kAsync;
    uint8_t irq_enable_mask_ = 0;
    uint8_t fifo_mode_value_ = 0;
    uint16_t acc_lsb_div_ = 0;
    uint16_t gyro_lsb_div_ = 0;
    uint32_t last_timestamp_ = 0;
    uint32_t revision_id_ = 0;
    uint8_t usid_[6] = {};
    uint8_t last_motion_status1_ = 0;
    std::vector<uint8_t> fifo_buffer_;

    QueueHandle_t interrupt2_isr_queue_ = nullptr;
    QueueHandle_t interrupt2_event_queue_ = nullptr;
    TaskHandle_t interrupt_task_handle_ = nullptr;
    InterruptCallback interrupt2_callback_;
    FifoSampleCallback fifo_sample_callback_;
    EventCallback wake_on_motion_callback_;
    EventCallback tap_callback_;
    EventCallback pedometer_callback_;
    EventCallback no_motion_callback_;
    EventCallback any_motion_callback_;
    EventCallback significant_motion_callback_;
    EventCallback gyro_data_ready_callback_;
    EventCallback accel_data_ready_callback_;
    EventCallback data_locking_callback_;
    std::mutex sample_mutex_;
    Qmi8658Sample latest_sample_ = {};
    float latest_temperature_c_ = 0.0f;
    bool latest_sample_valid_ = false;
    bool latest_temperature_valid_ = false;
};

inline Qmi8658::SensorStatus operator|(Qmi8658::SensorStatus lhs, Qmi8658::SensorStatus rhs) {
    return static_cast<Qmi8658::SensorStatus>(static_cast<uint16_t>(lhs) |
                                              static_cast<uint16_t>(rhs));
}

inline Qmi8658::SensorStatus& operator|=(Qmi8658::SensorStatus& lhs, Qmi8658::SensorStatus rhs) {
    lhs = lhs | rhs;
    return lhs;
}

#endif  // QMI8658_H
