# ESP32 Power Optimization (ESP-IDF)

Use this reference for ESP32 low-power modes, wakeup design, dynamic power management, and battery-aware behavior in ESP-IDF projects.

## Start With Power Budget and Wakeup Model

- Define target average current, active duty cycle, wakeup sources, and latency requirements first.
- Power tuning without a measurement plan usually produces misleading results.
- Identify whether the product is:
  - always-connected mains-powered
  - battery-powered periodic sensor
  - bursty wireless device
  - low-latency interactive device

## ESP32 Sleep Modes (Practical)

- Active mode: CPU/peripherals/radios running.
- Modem sleep: CPU active, radio duty-cycled/power-save behavior (Wi-Fi/BLE use case dependent).
- Light sleep: CPU paused with faster wake than deep sleep; RAM retained (chip/config dependent).
- Deep sleep: lowest-power common mode; most runtime state lost except RTC-retained data/configured wake sources.

Choose based on:
- required wake latency
- state retention needs
- radio reconnect cost
- sampling interval

## Wakeup Sources (ESP-IDF)

- Timer wakeup: `esp_sleep_enable_timer_wakeup(...)`
- GPIO/EXT wakeup (chip-specific APIs and limitations differ by target)
- ULP / coprocessor wakeup on supported chips
- Touch / UART wakeup on supported targets and configurations

Always verify target-specific wakeup support for your exact chip (`esp32`, `esp32s3`, `esp32c3`, etc.).

## Dynamic Frequency Scaling and PM Locks

- Prefer ESP-IDF power management APIs over manual clock manipulation.
- Use `esp_pm_configure(...)` for DFS/light-sleep policies where supported.
- Use PM locks (`esp_pm_lock_*`) only around operations that truly require a minimum frequency or no light sleep.
- Release locks promptly; leaked PM locks are a common reason “power save doesn’t work.”

## Wi-Fi / BLE Power Strategy

- Radio behavior often dominates power consumption.
- Optimize at the system level:
  - batching network activity
  - reducing reconnect churn
  - using appropriate Wi-Fi power-save mode
  - minimizing unnecessary scans/advertising activity
- Validate power impact of retry loops and error handling; “recover faster” can cost much more energy.

## Peripheral Power Management

- Deinit or stop unused peripherals/drivers when idle (ADC, SPI devices, sensors, UARTs if safe).
- Gate external sensors/rails with load switches when hardware allows.
- Avoid periodic polling when interrupt/event-based wakeup is feasible.
- Use DMA/queued transfers to reduce CPU wake time for bulk I/O.

## GPIO Leakage and Deep Sleep Considerations

- Configure unused pins to known safe states for your board design (high-Z, pull, or driven level depending leakage path).
- Check board-specific leakage via external pull-ups, level shifters, sensors, and transistor networks.
- Use RTC IO hold/isolation features where appropriate and supported.
- Beware strapping pins and boot requirements when changing default pin states.

## Battery Monitoring and Adaptive Behavior

- Use calibrated ADC measurements (ESP-IDF ADC calibration APIs) when voltage accuracy matters.
- Sample battery at controlled times (load state affects voltage).
- Define thresholds with hysteresis to avoid oscillation.
- Adapt workload:
  - sample less often
  - reduce radio activity
  - defer non-critical features

## Measurement and Verification

- Measure current with appropriate tools (power analyzer/current meter), not only software estimates.
- Compare:
  - idle active
  - light sleep
  - deep sleep
  - radio TX/RX peaks
  - reconnect storms / failure conditions
- Record exact firmware config (`sdkconfig`, target, board revision) with measurements.

## Review Checklist (Merged and ESP32-Adapted)

- Sleep mode chosen based on latency + retention + reconnect cost.
- Wakeup sources and chip-specific limitations verified.
- PM locks acquired only when needed and released correctly.
- Wireless retry/connect logic reviewed for energy impact.
- Peripheral/sensor idle states and external rail control considered.
- GPIO leakage paths and strapping pin states reviewed.
- Battery thresholds use hysteresis and calibrated ADC path when needed.
- Power claims backed by measurement, not estimates alone.
