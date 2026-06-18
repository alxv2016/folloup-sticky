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
- A ported PCF8563 RTC driver.
- A `board` component for Sticky-specific power, charger, ADC, and BQ27220
  and RTC wiring.
- A `power_service` component that initializes power hardware and logs a
  diagnostic power/battery/RTC snapshot.
- A `button_service` component that logs app-facing button events through
  Espressif's managed button component.
- A `buzzer_service` component that owns PWM buzzer setup and app-facing sound
  patterns.
- A ported `sd_card` component for SDSPI/FATFS MicroSD access.
- A `storage_service` component that owns app-facing MicroSD mount and debug
  status policy.
- A ported mono SSD1677 e-paper panel driver.
- A `display_service` component that owns app-facing e-paper bring-up and the
  first portrait "Hello world" screen.

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
  buzzer_service/
    include/
      buzzer_service.h
    buzzer_service.cpp
  storage_service/
    include/
      storage_service.h
    storage_service.cpp
  display_service/
    include/
      display_service.h
    display_service.cpp
  sd_card/
    include/
      sd_card.h
    sd_card.cpp
  epaper_panel/
    include/
      epaper_panel.h
    epaper_panel.cpp
    ssd1677_driver.cpp
  bq27220/
    include/
      bq27220.h
    priv_include/
      bq27220_reg.h
    bq27220.cpp
  pcf8563/
    include/
      pcf8563.h
    pcf8563.cpp
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
- Initializes `buzzer_service` and requests the startup pattern.
- Initializes `display_service` and draws the initial e-paper screen.
- Initializes `storage_service` and logs one MicroSD diagnostic snapshot.
- Initializes `button_service`.
- Subscribes to button events and logs app-level power-button shutdown intent.
- Runs a small shutdown task so button callbacks can request shutdown without
  directly executing the power-latch release sequence.

Power-button long press start arms shutdown, and long-press release requests it.
This avoids releasing the latch while the physical power button is still being
held. The shutdown task also waits briefly after release before calling
`power_service::RequestShutdown()` so the analog button/Q2 bootstrap path has
time to stop feeding `PWR_EN`. The button callback only notifies the AppShell
shutdown task; the task calls the power service so latch-release timing does not
run inside the button callback.

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

### `components/pcf8563`

This is the generic PCF8563 RTC driver ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo/components/pcf8563
```

The driver should stay board-agnostic. It works from an initialized
`i2c_master_dev_handle_t` and should not own Sticky-specific GPIO numbers or
I2C ports.

Current scope:

- probe the RTC at address `0x51`
- disable CLKOUT
- read and set date/time
- read, clear, and disable alarm/timer interrupt state

The interrupt helpers are an app-specific extension beyond the source demo's
basic time read/write helpers. They exist because schematic page 5 ties
`VDD_3V3_ENn` to `RTC_INTn`, so the shutdown path needs a way to clear an
asserted RTC interrupt before releasing the latch.

### `components/board`

This component centralizes Sticky-specific hardware access for the current power
scope.

`sticky_board_config.h` owns:

- power latch data / `PWR_HOLD`: `GPIO_NUM_45`
- power latch clock / `PWR_LOCK`: `GPIO_NUM_46`
- power / OK button: `GPIO_NUM_4`
- up button: `GPIO_NUM_5`
- down button: `GPIO_NUM_6`
- buzzer PWM output: `GPIO_NUM_48`
- MicroSD power enable: `GPIO_NUM_10`
- MicroSD card detect: `GPIO_NUM_11`
- MicroSD chip select: `GPIO_NUM_8`
- MicroSD SPI clock: `GPIO_NUM_13`
- MicroSD SPI MOSI/CMD: `GPIO_NUM_14`
- MicroSD SPI MISO/D0: `GPIO_NUM_12`
- shared SPI host: `SPI2_HOST`
- shared SPI clock: `GPIO_NUM_13`
- shared SPI MOSI: `GPIO_NUM_14`
- shared SPI MISO: `GPIO_NUM_12`
- e-paper power enable: `GPIO_NUM_47`
- e-paper busy: `GPIO_NUM_18`
- e-paper reset: `GPIO_NUM_17`
- e-paper data/command: `GPIO_NUM_16`
- e-paper chip select: `GPIO_NUM_15`
- charger enable: `GPIO_NUM_39`, active low
- charger state: `GPIO_NUM_40`
- power-input ADC sense: `GPIO_NUM_9`
- sensor I2C bus port: `I2C_NUM_1`
- sensor I2C SCL: `GPIO_NUM_0`
- sensor I2C SDA: `GPIO_NUM_1`
- BQ27220 I2C address: `0x55`
- BQ27220 interrupt pin: `GPIO_NUM_7`
- PCF8563 I2C address: `0x51`
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
- `sticky_board::EnsureSharedSpiBus()`
- `sticky_board::EnableEpaperPower()`
- `sticky_board::CreateSensorI2cBus(...)`
- `sticky_board::AddBq27220Device(...)`
- `sticky_board::AddPcf8563Device(...)`

Keep this layer focused on raw board mechanics: pins, buses, GPIO polarity, ADC
setup, and latch timing.

Power-latch GPIOs are configured as input/output during bring-up so firmware can
both drive `PWR_HOLD` / `PWR_LOCK` and log the observed pad levels for hardware
debugging. Startup follows the Seeed peripheral demo behavior and drives both
`PWR_HOLD` and `PWR_LOCK` high to keep the board alive after the physical power
button is released. For shutdown testing, the latest Page 6 trace treats
`PWR_HOLD` as Q2's gate, Q2 as the path that feeds `PWR_EN`, and U3 Q as the
signal that drives Q7. The current hard-off attempt first pulses `PWR_LOCK` with
`PWR_HOLD` low to latch U3 Q low and release Q7, then drives `PWR_HOLD` high to
try to turn Q2 off before falling back to soft-off if the rail remains powered.
Before each latch sequence, the board layer disables ESP-IDF GPIO hold/deep-sleep
hold behavior for GPIO45/GPIO46 and resets both pads before reconfiguring them.
This is intentional because both pins are strapping-sensitive and power-latch
debugging needs to rule out stale pad or sleep-hold state.

The current schematic trace does not show `VDD_3V3_ENn` routed to an ESP32-S3
GPIO. Page 5 shows it tied at the top level to `RTC_INTn`, which is powered from
the always-on RTC rail. Treat hard power-off as latch/RTC-controlled through
`PWR_HOLD`, `PWR_LOCK`, and the RTC interrupt path unless a future board revision
or netlist proves a direct buck-boost enable GPIO exists.

Known real power-off issue:

- True rail-cut power-off is not currently working through firmware.
- Shutdown sequences tested so far include:
  - `PWR_HOLD=0`, `PWR_LOCK` low-to-high pulse, then `PWR_LOCK=0`
  - the same pulse sequence after waiting for physical `POWER_OK` release
  - the same sequence followed by placing GPIO45/GPIO46 in input/no-pull mode
  - the vendor-demo inverse behavior: drive both `PWR_HOLD=0` and `PWR_LOCK=0`
    and keep both low
  - the alternate Page 6 Q2-gate interpretation: keep `PWR_LOCK=0` and hold
    `PWR_HOLD=1`
- All tested sequences left firmware running afterward on this board.
- The current hard-off experiment combines the two Page 6 mechanisms: pulse
  `PWR_LOCK` while `PWR_HOLD=0`, then hold `PWR_HOLD=1`.
- The board layer also disables GPIO hold/deep-sleep hold and resets the latch
  pads before release attempts, so stale ESP-IDF GPIO hold state has been ruled
  out as the likely cause.
- The Page 6 trace shows an ungated D2 path from `VIN_5V` to `PWR_EN`, so USB
  can independently keep `PWR_EN` asserted while plugged in. Battery-only testing
  still stayed powered, so USB/VBUS backfeed is not the only hard-off blocker.
- Page 5 ties `VDD_3V3_ENn` to `RTC_INTn`; if the always-on RTC interrupt is
  asserted low, it may keep or re-enable the main 3.3 V rail. True hard-off work
  should include clearing/disabling RTC interrupt flags before releasing the
  latch.
- Current product behavior is therefore soft-off: attempt the latch release,
  then enter ESP32 deep sleep if the rail remains alive.
- To resume true hard power-off work, we need a confirmed schematic netlist,
  vendor firmware sequence, RTC shutdown sequence, or board-revision note
  explaining how U3/Q7/PWR_EN/RTC_INTn are intended to collapse `VDD_3V3`.

### `components/power_service`

This component is the app-facing power layer. It composes the `board` helpers
with the BQ27220 and PCF8563 drivers.

Current responsibilities:

- expose `power_service::EnablePowerHold()` so `main` can assert power hold as
  the first application action
- configure charger pins and enable charging
- initialize power-input ADC sensing
- initialize the sensor I2C bus, PCF8563 device, and BQ27220 device
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
- PCF8563 control/status-2 bits for alarm/timer flags and interrupt enables

`power_service::RequestShutdown()` is the app-facing shutdown entry point. It is
currently called by AppShell after a `POWER_OK` long press. It first clears and
disables PCF8563 alarm/timer interrupt sources so `RTC_INTn` is not intentionally
holding `VDD_3V3_ENn` low, then attempts the Sticky hardware latch release. If
firmware is still running after that release returns, the service enters ESP32
deep sleep as a soft-off fallback with `POWER_OK` / `GPIO4` configured as an
active-low wake source. Before arming that wake source, the service waits for
`POWER_OK` to be high/stable so the device does not immediately wake from an
already-active button line.

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
- exposes a typed event callback API for app-level policy routing in
  `app_shell`

Power-save button wake is intentionally disabled for this first pass. Before
enabling button wake from light sleep, verify whether the managed component
version includes the GPIO power-save ISR safety behavior noted in the reference
demo's patched vendored component.

### `components/buzzer_service`

This C++ component owns app-facing buzzer feedback. It uses ESP-IDF LEDC PWM on
the Sticky buzzer pin and hides timer/channel/duty details from `main`.

Current scope:

- `BUZZER_PWM` on `GPIO48`
- LEDC low-speed mode
- LEDC timer 0 and channel 0
- 10-bit duty resolution
- asynchronous command queue and worker task
- `PlayTone(...)`, `PlayPattern(...)`, and `Stop()`
- named startup, click, long-click, double-click, error, and shutdown patterns

AppShell maps all button single-click, double-click, and long-press-start events
to click, double-click, and long-click buzzer patterns. The shutdown task also
requests the shutdown pattern before waiting for button-release settle and
calling `power_service::RequestShutdown()`.

The service drives tones at a 50 percent PWM duty cycle, which is the loudest
useful square-wave drive for this passive PWM buzzer. A 100 percent duty cycle
would be DC and would not produce the intended tone.

AppShell may request patterns such as startup or shutdown, but it should not
know about LEDC timer numbers, PWM duty values, or GPIO setup.

### `components/sd_card`

This is the SDSPI/FATFS MicroSD wrapper ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo/components/sd_card
```

The component is mostly board-agnostic. It receives an `SdCardPins` struct and
mount point from its caller, then owns:

- SD power-enable GPIO configuration
- card-detect GPIO configuration
- SDSPI bus/device setup, unless the caller marks the SPI bus as externally
  owned
- FATFS mount/unmount at the requested mount point
- storage statistics
- directory listing
- small file read/write/append/truncate helpers

Do not make this component depend on `board`; pass pins in from the service or
board layer. On Sticky, `storage_service` asks the board layer to initialize the
shared SPI bus first and passes `external_spi_bus=true`, so the SD wrapper only
adds its SDSPI device to the existing bus.

### `components/storage_service`

This is the app-facing storage layer. It composes `board` pin definitions with
the `sd_card` wrapper.

Current scope:

- use the schematic page 5 MicroSD pin map
- check `SD_DETECT`
- mount `/sdcard` when a card is present
- log mount status, total/free bytes, and a small root directory preview
- write/read `/sdcard/sticky_sd_probe.txt` once as a bring-up probe
- leave formatting disabled by default

An absent SD card is not a fatal app startup error. Mount failures are logged
and returned to AppShell as non-fatal service initialization failures.

MicroSD shares SPI lines with the e-paper path:

- `SD_CLK/SCK` / `EP_SCK`: `GPIO13`
- `SD_CMD/MOSI` / `EP_SDI`: `GPIO14`
- `SD_D0/MISO`: `GPIO12`
- SD card chip select: `GPIO8`
- e-paper chip select: `GPIO15`

`storage_service` must call `sticky_board::EnsureSharedSpiBus()` before mounting
the SD card. Shared SPI bus ownership belongs in `board`, not in `sd_card`,
`epaper_panel`, `storage_service`, or `display_service`.

### `components/epaper_panel`

This is the raw mono SSD1677 e-paper panel driver ported from:

```text
/Users/tieuvong/Development/followup/components/board_drivers/epaper_panel
```

The driver should stay board-agnostic. It receives an `EpaperPanelConfig` from
its caller and owns:

- e-paper reset, busy, data/command, and chip-select GPIO control
- the SSD1677 command/data write path
- the mono framebuffer
- the retained previous framebuffer used by partial refresh
- full base refresh
- raw-coordinate region partial refresh
- whole-screen mono partial refresh
- panel sleep
- refresh timing metrics

Current scope is intentionally mono-only. Do not port gray4 support unless a
future product requirement explicitly asks for it.

The first display update must use `RefreshFullBase()` so the SSD1677 current and
previous RAM planes are seeded. Later full-screen mono updates may use
`RefreshPartialFullScreen()`, which delegates to `RefreshPartialRegion()` with
the full panel bounds. Region partial refresh takes raw framebuffer pixel
coordinates with exclusive end bounds and internally byte-aligns the X range for
1 bpp transfers. If partial refresh is requested before a base image exists,
after sleep/timeout, or after the partial-refresh limit, the raw driver falls
back to `RefreshFullBase()`.

The driver can initialize its own SPI bus for standalone reuse, but Sticky code
must pass `external_spi_bus=true` after `sticky_board::EnsureSharedSpiBus()` has
initialized the shared `SPI2_HOST` bus.

Not yet ported from Folloup:

- wake API and display wake policy
- fast refresh/base path
- retained view dirty-region policy
- logical-to-raw display view abstraction

### `components/display_service`

This is the app-facing display layer. It composes `board` pin definitions and
power helpers with the `epaper_panel` raw driver.

Current scope:

- initialize the shared SPI bus through `sticky_board::EnsureSharedSpiBus()`
- enable e-paper panel power through `sticky_board::EnableEpaperPower()`
- initialize the raw SSD1677 panel driver
- draw a portrait "Hello world" screen for bring-up
- perform the first `RefreshFullBase()` and log panel metrics

`display_service` owns app-facing display policy. Driver-specific wiring and
SSD1677 commands must stay out of `main`. Raw board pin ownership stays in
`board`, and low-level SSD1677 command sequencing stays in `epaper_panel`.

## Hardware Notes

- Main controller: `ESP32-S3R8`.
- External flash: 256 Mbit / 32 MB QSPI flash.
- PSRAM: 8 MB octal PSRAM.
- BQ27220 address: `0x55`.
- PCF8563 address: `0x51`.
- Buzzer PWM output: `GPIO48`.
- MicroSD uses SDSPI mode only: `SD_CLK/SCK` on `GPIO13`, `SD_CMD/MOSI` on
  `GPIO14`, `SD_D0/MISO` on `GPIO12`, `SD_D3/CS` on `GPIO8`, `SD_PWR_EN` on
  `GPIO10`, and `SD_DETECT` on `GPIO11`. `SD_D1` and `SD_D2` are not connected.
- The SSD1677 e-paper panel shares `SPI2_HOST` with MicroSD: `EP_SCK` on
  `GPIO13`, `EP_SDI/MOSI` on `GPIO14`, `EP_SDO/MISO` on `GPIO12`, `EP_CS` on
  `GPIO15`, `EP_DC` on `GPIO16`, `EP_RST` on `GPIO17`, `EP_BUSY` on `GPIO18`,
  and `EP_PWR_EN` on `GPIO47`.
- The e-paper panel is 800 x 480 raw landscape pixels. The bring-up
  `display_service` draws portrait content by mapping logical 480 x 800
  coordinates into the raw SSD1677 framebuffer.
- Power latch uses `PWR_HOLD` on `GPIO45` as U3 D and `PWR_LOCK` on `GPIO46`
  as U3 CP. Firmware sets the desired D value and pulses CP to latch it.
- `VDD_3V3_ENn` is not currently mapped to a firmware GPIO, so there is no
  confirmed independent software kill pin for the 3.3 V buck-boost rail. Page 5
  ties it to `RTC_INTn`, so the RTC interrupt state is part of the hard-off path.
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
       -> pcf8563 -> ESP-IDF I2C driver
  -> button_service -> espressif/button
  -> buzzer_service -> board -> ESP-IDF LEDC driver
  -> storage_service
       -> board
       -> sd_card -> ESP-IDF SDSPI/FATFS/SDMMC drivers
  -> display_service
       -> board
       -> epaper_panel -> ESP-IDF SPI/GPIO drivers
```

Avoid making `bq27220`, `pcf8563`, `sd_card`, or `epaper_panel` depend on
`board`; that would make generic drivers board-specific.
