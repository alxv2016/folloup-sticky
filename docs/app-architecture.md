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
- A small `board` component scoped only to BQ27220 wiring and I2C setup.

The rest of the board peripherals have not been ported yet.

## Project Layout

```text
CMakeLists.txt
main/
  CMakeLists.txt
  main.cpp
components/
  board/
    include/
      sticky_board_config.h
      sticky_board.h
    sticky_board.cpp
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

`main/main.cpp` is intentionally minimal. It currently only performs the
required OTA rollback validation hook:

- Detects whether the running image is `ESP_OTA_IMG_PENDING_VERIFY`.
- Marks the image valid with `esp_ota_mark_app_valid_cancel_rollback()`.

Driver-specific wiring should stay out of `main` until an integration step is
intentional.

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

This component centralizes only the BQ27220-related board wiring for now.

`sticky_board_config.h` owns:

- sensor I2C bus port: `I2C_NUM_1`
- sensor I2C SCL: `GPIO_NUM_0`
- sensor I2C SDA: `GPIO_NUM_1`
- BQ27220 I2C address: `0x55`
- BQ27220 interrupt pin: `GPIO_NUM_7`
- I2C glitch filter and bus speed constants

`sticky_board.h/.cpp` owns small board helper functions:

- `sticky_board::CreateSensorI2cBus(...)`
- `sticky_board::AddBq27220Device(...)`

Keep this layer narrow until more board hardware is intentionally ported.

## Hardware Notes

- Main controller: `ESP32-S3R8`.
- External flash: 256 Mbit / 32 MB QSPI flash.
- PSRAM: 8 MB octal PSRAM.
- BQ27220 address: `0x55`.
- BQ27220 shares the sensor I2C bus with other future peripherals.
- Sensor I2C uses `GPIO0` for SCL and `GPIO1` for SDA.
- `GPIO0` is also an ESP32-S3 boot strapping/download pin. Create the sensor
  I2C bus only after boot has completed and startup pin levels are no longer
  part of the boot-mode decision.
- `GPIO7` is the BQ27220 interrupt line in the current scope. The hardware spec
  also notes this line is shared with the IMU interrupt path, so future IMU work
  must coordinate ownership.

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
  -> board
  -> bq27220
  -> ESP-IDF drivers
```

Avoid making `bq27220` depend on `board`; that would make a generic IC driver
board-specific.

## Next Likely Integration Step

When BQ27220 is wired into the app, the expected flow is:

1. Call `sticky_board::CreateSensorI2cBus(...)`.
2. Call `sticky_board::AddBq27220Device(...)`.
3. Call `bq27220_probe(...)`.
4. Read telemetry using direct helper APIs.
5. Decide later whether to configure/profile the gauge with `bq27220_create()`.

Keep that integration small and observable, with clear `esp_err_t` handling.
