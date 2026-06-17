# Folloup Sticky App Architecture

This project is an ESP-IDF C++17 firmware port for the Seeed reTerminal Sticky,
based on the local hardware spec in
`docs/reTerminal_Sticky_Hardware_Spec_Software_Porting-en.md`.

## Current Scope

The repository is currently a minimal ESP-IDF application scaffold with:

- ESP32-S3 target configuration.
- 32 MB QSPI flash configuration.
- 8 MB octal PSRAM configuration.
- OTA-ready partition layout with rollback enabled.
- A minimal C++ `app_main()`.
- A ported BQ27220 fuel-gauge driver.
- A `board` component for Sticky-specific power, charger, ADC, and BQ27220
  wiring.
- A `power_service` component that initializes power hardware and logs a
  diagnostic power/battery snapshot.
- A `button_service` component that logs app-facing button events through
  Espressif's managed button component.

The rest of the board peripherals have not been ported yet.

## Project Layout

```text
CMakeLists.txt
main/
  CMakeLists.txt
  main.cpp
  app_shell.h
  app_shell.cpp
components/
  board/
    include/
      sticky_board_config.h
      sticky_board.h
    sticky_board.cpp
  power_service/
    include/
      power_service.h
    power_service.cpp
  button_service/
    include/
      button_service.h
    button_service.cpp
  bq27220/
    include/
      bq27220.h
    priv_include/
      bq27220_reg.h
    bq27220.cpp
partitions.csv
sdkconfig
sdkconfig.defaults
docs/
  app-architecture.md
  reTerminal_Sticky_Hardware_Spec_Software_Porting-en.md
```

## Component Boundaries

### `main`

`main/` owns product composition for this firmware. It is not a reusable
component. Keep it focused on startup ordering and app-level orchestration.

`main/main.cpp` is intentionally tiny: it is only the ESP-IDF `app_main()` entry
point and delegates to `app_shell::Run()`.

`main/app_shell.cpp` is an orchestration layer only. It may decide startup order,
connect app-level policies, and choose whether an optional service failure is
fatal, but it should not contain hardware driver logic, protocol logic, button
debouncing, battery math, display drawing, networking workflows, or long-running
feature loops. Put those behaviors in services/components and call them from the
app shell.

The current early startup sequence is:

- Detects whether the running image is `ESP_OTA_IMG_PENDING_VERIFY`.
- Marks the image valid with `esp_ota_mark_app_valid_cancel_rollback()`.
- Asserts the Sticky power latch before OTA validation.
- Initializes `power_service`.
- Logs one power/battery diagnostic snapshot.
- Initializes `button_service`.

Driver-specific wiring should stay out of `main/`; app startup should call
service-level APIs instead. Add product-specific sequencing in `app_shell`, not
inside reusable components.

### `components/bq27220`

This is the generic BQ27220 battery fuel-gauge driver ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo/components/bq27220
```

The driver should stay board-agnostic. It works from an initialized
`i2c_master_dev_handle_t` and should not own Sticky-specific GPIO numbers or
I2C ports.

The driver exposes two usage styles:

- lightweight direct helpers such as `bq27220_probe()` and
  `bq27220_read_voltage_mv()`
- profile/configuration handle APIs such as `bq27220_create()`

For early bring-up, prefer the lightweight direct-helper flow used by the
source demo: create I2C bus, add BQ27220 device, probe, then read telemetry.

### `components/board`

This component centralizes Sticky-specific hardware access for the current power
scope.

`sticky_board_config.h` owns:

- power latch hold: `GPIO_NUM_45`
- power latch lock/control: `GPIO_NUM_46`
- power / OK button: `GPIO_NUM_4`
- up button: `GPIO_NUM_5`
- down button: `GPIO_NUM_6`
- charger enable: `GPIO_NUM_39`, active low
- charger state: `GPIO_NUM_40`
- power-input ADC sense: `GPIO_NUM_9`
- sensor I2C bus port: `I2C_NUM_1`
- sensor I2C SCL: `GPIO_NUM_0`
- sensor I2C SDA: `GPIO_NUM_1`
- BQ27220 I2C address: `0x55`
- BQ27220 interrupt pin: `GPIO_NUM_7`
- I2C glitch filter and bus speed constants

`sticky_board.h/.cpp` owns small board helper functions:

- `sticky_board::EnablePowerHold()`
- `sticky_board::ReleasePowerHold()`
- `sticky_board::ConfigureChargerPins()`
- `sticky_board::SetChargerEnabled(...)`
- `sticky_board::ReadChargeState(...)`
- `sticky_board::InitPowerInputSense()`
- `sticky_board::ReadPowerInputSample(...)`
- `sticky_board::ConfigureBq27220InterruptPin()`
- `sticky_board::ReadBq27220InterruptLevel(...)`
- `sticky_board::CreateSensorI2cBus(...)`
- `sticky_board::AddBq27220Device(...)`

Keep this layer focused on raw board mechanics: pins, buses, GPIO polarity, ADC
setup, and latch timing.

Power-latch GPIOs are configured as input/output during bring-up so firmware can
both drive `PWR_HOLD` / `PWR_LOCK` and log the observed pad levels for hardware
debugging.

### `components/power_service`

This component is the app-facing power layer. It composes the `board` helpers
with the BQ27220 driver.

Current responsibilities:

- expose `power_service::EnablePowerHold()` so `main` can assert power hold as
  the first application action
- configure charger pins and enable charging
- initialize power-input ADC sensing
- initialize the sensor I2C bus and BQ27220 device
- expose `power_service::ReadStatus(...)`
- log one diagnostic snapshot through `power_service::LogDebugStatus()`

The current diagnostic snapshot includes:

- service initialization state
- charger enabled state
- charger GPIO state
- averaged power-input ADC raw min/max/average plus calibrated sense voltage
  when ADC calibration is available
- USB/external-power detection using a conservative sense-pin threshold
- BQ27220 battery telemetry when the gauge is available
- BQ27220 full-charge status bit
- low-battery-at-10-percent status derived from BQ27220 state of charge
- BQ27220 operation status, BTP thresholds, and initial `BFG_INT` level

`power_service::RequestShutdown()` exists as the app-facing shutdown entry
point, but `main` does not call it. Shutdown should only be wired to UX/policy
after the intended button behavior is defined.

### `components/button_service`

This C++ component owns app-facing button initialization and logging. It uses
Espressif's managed `espressif/button` component for the underlying debounce and
button-event state machine.

Current scope:

- `POWER_OK` on `GPIO4`
- `UP` on `GPIO5`
- `DOWN` on `GPIO6`
- active-low GPIO buttons with internal pulls enabled by the managed component
- logs press down, press up, single click, double click, long press start, and
  long press up

Power-save button wake is intentionally disabled for this first pass. Before
enabling button wake from light sleep, verify whether the managed component
version includes the GPIO power-save ISR safety behavior noted in the reference
demo's patched vendored component.

## Hardware Notes

- Main controller: `ESP32-S3R8`.
- External flash: 256 Mbit / 32 MB QSPI flash.
- PSRAM: 8 MB octal PSRAM.
- BQ27220 address: `0x55`.
- Power latch uses `PWR_HOLD` on `GPIO45` and `PWR_LOCK` on `GPIO46`.
- Charger enable is active low on `GPIO39`.
- Charger state is read from `GPIO40`; the reference demo treats low as
  charging.
- Power-input voltage is sensed on `GPIO9`. The service currently logs ADC pin
  millivolts, not reconstructed VIN, because the divider ratio has not been
  confirmed in this project.
- BQ27220 shares the sensor I2C bus with other future peripherals.
- Sensor I2C uses `GPIO0` for SCL and `GPIO1` for SDA.
- `GPIO0` is also an ESP32-S3 boot strapping/download pin. Create the sensor
  I2C bus only after boot has completed and startup pin levels are no longer
  part of the boot-mode decision.
- `GPIO7` is the BQ27220 interrupt line in the current scope. The hardware spec
  also notes this line is shared with the IMU interrupt path, so future IMU work
  must coordinate ownership. Do not add an IMU interrupt handler that claims
  GPIO7 independently of the power/fuel-gauge path.

## Configuration

Configuration is file-based and should stay reproducible:

- `sdkconfig.defaults` captures the intended project defaults.
- `sdkconfig` captures the resolved ESP-IDF configuration.
- `partitions.csv` defines the OTA partition table.

The partition table currently contains:

- `nvs`
- `otadata`
- `phy_init`
- `ota_0`
- `ota_1`

Rollback is enabled with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Because
rollback is enabled, the application must keep the OTA validation hook in
`app_main()` or equivalent early startup code.

## Dependency Direction

Use this dependency direction:

```text
app / integration code
  -> power_service
       -> board -> ESP-IDF drivers
       -> bq27220 -> ESP-IDF I2C driver
  -> button_service -> espressif/button
```

Avoid making `bq27220` depend on `board`; that would make a generic IC driver
board-specific.
