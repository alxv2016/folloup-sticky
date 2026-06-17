# ESP32 Peripheral Programming (ESP-IDF)

Use this reference for ESP32 GPIO, interrupts, timers, ADC, PWM, watchdogs, and low-level programming decisions in ESP-IDF.

## ESP32-Specific Default

- Prefer ESP-IDF drivers and HAL-style APIs first.
- Avoid direct register programming unless:
  - the project already does it
  - a required feature is unavailable in the driver
  - there is a measured performance/timing reason
- If direct registers are used, isolate them behind a component API and document chip assumptions.

## GPIO Configuration

- Use `gpio_config()` with explicit mode, pull, and interrupt settings.
- Validate pin capability on the selected target (input-only pins, analog-capable pins, strapping pins, RTC IO availability).
- Prefer named constants and board definitions over raw GPIO numbers scattered across code.

Basic pattern:
- board pin map header
- one init function per subsystem
- no hidden reconfiguration in unrelated modules

## GPIO Interrupts

- Use `gpio_install_isr_service()` and `gpio_isr_handler_add()` for GPIO ISR wiring.
- Mark ISR handlers `IRAM_ATTR` when required by the configured interrupt path.
- Keep ISR handlers minimal: timestamp, latch state, notify task, return.
- Debounce in task context or timer context, not by blocking in ISR.

## Timer Choices (ESP-IDF)

- `esp_timer`: software callbacks, high-resolution scheduling.
- `gptimer`: hardware timer/capture/compare use cases.
- FreeRTOS timers: non-precise app-level timing/retries.

Do not force one timer type for all problems. Pick based on precision, callback context, and CPU load.

## PWM and Pulse Output

- Prefer LEDC for common PWM use cases (LED dimming, simple PWM outputs).
- Use MCPWM for motor-control-class needs where relevant and supported.
- Validate timer resolution/frequency tradeoffs explicitly.

## ADC Patterns

- Prefer ESP-IDF ADC drivers (oneshot/continuous, version-dependent APIs) and calibration helpers when voltage accuracy matters.
- Be explicit about attenuation, sampling conditions, and calibration source.
- Avoid assuming lab-bench voltage readings match in-field under load/noise.
- Separate “raw sensor read” from “engineering units conversion” in code for easier testing.

## UART / Serial Logging Integration

- Keep application protocol UART handling separate from console/log UART assumptions.
- If using `idf.py monitor` for logs, document baud and port assumptions in debug steps.
- Avoid flooding logs in tight loops; it distorts timing and can mask race/watchdog issues.

## Watchdogs (Practical)

- Use task watchdogs and system watchdogs intentionally; do not disable them to hide starvation issues.
- Feed watchdogs in the owner task/main loop path, not in random helper functions.
- When watchdog resets occur, inspect:
  - blocking calls
  - deadlocks
  - ISR storms
  - long critical sections
  - log flooding / busy waits

## Clocking and Timing (ESP32)

- Clock/frequency behavior is largely configured through ESP-IDF and `sdkconfig`; avoid manual clock-tree style code from other MCUs.
- For timing-sensitive code, measure actual intervals (`esp_timer_get_time()`, timestamps, scope/logic analyzer) instead of assuming nominal frequency.
- For throughput-sensitive peripherals (display/storage/streaming), review flash/PSRAM mode, bus clock, DMA usage, and memory placement together; the best option is hardware- and board-dependent.

## Low-Power Programming Cross-Reference

- For sleep/wakeup and power strategy, read `references/power-optimization.md`.
- For communication bus timing and DMA concerns, read `references/communication-protocols.md`.

## Review Checklist (Merged and ESP32-Adapted)

- ESP-IDF drivers used unless a justified low-level exception exists.
- Pin capabilities and strapping constraints checked for target chip.
- GPIO ISR handlers are minimal, ISR-safe, and IRAM-safe where required.
- Correct timer subsystem selected (`esp_timer`, `gptimer`, FreeRTOS timer, LEDC/MCPWM).
- ADC attenuation/calibration assumptions documented.
- Watchdog handling preserves diagnostics instead of masking issues.
- Timing-sensitive behavior validated by measurement/logging, not assumptions.
