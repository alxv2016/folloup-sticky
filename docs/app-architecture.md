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
- A `buzzer_service` component that owns PWM buzzer setup and low-level sound
  patterns.
- A `feedback_service` component that owns app-facing interaction feedback
  policy and maps app events onto buzzer patterns.
- A `design_tokens` component that owns shared product UI constants such as
  spacing, colors, typography roles, and component sizing.
- An `epaper_ui` component that owns reusable e-paper presentation primitives
  such as the status bar renderer, lock-screen renderer, and future view
  widgets being ported from the old app.
- A ported `sd_card` component for SDSPI/FATFS MicroSD access.
- A `storage_service` component that owns app-facing MicroSD mount, format, and
  debug status policy.
- A `wifi_service` component that owns ESP-IDF Wi-Fi station/AP lifecycle,
  saved credentials, scan state, and the backend HTTP routes for setup/status.
- A `timezone_service` component that owns timezone settings, SNTP sync,
  system-time updates, PCF8563 RTC writeback, and backend HTTP routes for time
  settings/runtime state.
- A `gemini_service` component that owns Gemini API key settings precedence,
  backend HTTP routes, and Gemini authentication readiness state.
- An input-only `pdm_mic` component that owns ESP-IDF I2S PDM RX capture.
- A `microphone_service` component that owns Sticky microphone pin mapping,
  microphone power/read lifecycle, and input-level calculation.
- A `recording_service` component that owns voice-input recording state,
  pre-roll buffering, PSRAM-backed clips, input-level tracking, and WAV export
  to MicroSD.
- A ported mono SSD1677 e-paper panel driver.
- A `display_service` component that owns app-facing e-paper bring-up, blank
  screen refresh, display sleep, and light-sleep recovery.
- Staged e-paper asset generation scripts and source PNG/TTF assets for the
  upcoming app UI.
- A ported GT911 capacitive touch controller driver.
- A `touch_service` component that owns app-facing touch bring-up, interrupt
  servicing, and touch event logging.
- A ported LSM6DS3 / LSM6DS3TR-C inertial sensor driver.
- An `imu_service` component that owns app-facing IMU bring-up and direct sample
  logging for first hardware validation.
- A `device_sleep_service` component that owns auto-sleep policy state,
  inactivity timing, app-level blocker checks, and staged sleep events.
- A `task_config` component that owns the app-created FreeRTOS task priority
  and core-affinity mapping.
- A ported SHT40 temperature/humidity sensor driver.
- An `environment_service` component that owns app-facing ambient
  temperature/humidity bring-up and sample logging.

The rest of the board peripherals have not been ported yet.

## Project Layout

```text
CMakeLists.txt
assets/
  epaper_assets.json
  icons/
  logos/
fonts/
main/
  CMakeLists.txt
  main.cpp
  app_shell.h
  app_shell.cpp
  device_sleep_runtime.h
  device_sleep_runtime.cpp
  status_bar_runtime.h
  status_bar_runtime.cpp
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
  feedback_service/
    include/
      feedback_service.h
    feedback_service.cpp
  design_tokens/
    include/
      design_tokens.h
  epaper_ui/
    include/
      epaper_ui/
        bitmap_font.h
        font_renderer.h
        generated_epaper_fonts.h
        status_bar.h
    bitmap_font.cpp
    font_renderer.cpp
    generated_epaper_fonts.cpp
    status_bar.cpp
  project_assets/
    CMakeLists.txt
    asset_manifest.h
    asset_types.h
    generated_epaper_footer_icons.h
    generated_epaper_footer_icons.cpp
    generated_epaper_icons.h
    generated_epaper_icons.cpp
    generated_epaper_logos.h
    generated_epaper_logos.cpp
    project_assets.h
    project_assets.cpp
  storage_service/
    include/
      storage_service.h
    storage_service.cpp
  gemini_service/
    include/
      gemini_service.h
    gemini_service.cpp
  wifi_service/
    include/
      wifi_service.h
    wifi_service.cpp
  timezone_service/
    include/
      timezone_service.h
    timezone_service.cpp
  pdm_mic/
    include/
      pdm_mic.h
    pdm_mic.cpp
  microphone_service/
    include/
      microphone_service.h
    microphone_service.cpp
  recording_service/
    include/
      recording_service.h
    recording_service.cpp
  display_service/
    include/
      display_service.h
    display_service.cpp
  touch_service/
    include/
      touch_service.h
    touch_service.cpp
  imu_service/
    include/
      imu_service.h
    imu_service.cpp
  task_config/
    include/
      followup_task_config.h
  environment_service/
    include/
      environment_service.h
    environment_service.cpp
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
  gt911/
    include/
      gt911.h
    gt911.cpp
  lsm6ds3/
    include/
      lsm6ds3.h
    priv_include/
      lsm6ds3_reg.h
    lsm6ds3.cpp
  sht40/
    include/
      sht40.h
    sht40.cpp
partitions.csv
sdkconfig
sdkconfig.defaults
docs/
  asset-generation.md
  app-architecture.md
  gemini-service.md
  display-demo-cleanup.md
  reTerminal_Sticky_Hardware_Spec_Software_Porting-en.md
scripts/
  generate_epaper_assets_common.py
  generate_epaper_footer_icons.py
  generate_epaper_fonts.py
  generate_epaper_icons.py
  generate_epaper_logos.py
  generate_epaper_project_assets.py
```

## Component Boundaries

### `components/project_assets`

This component owns embedded assets that are compiled into the firmware image.
The source PNG/TTF files and generator scripts live outside the component; this
component exposes only the generated C++ data and a small app-facing lookup API.

Current scope:

- packed monochrome image metadata through `asset_types.h`
- manifest-driven asset generation through `assets/epaper_assets.json`
- generated e-paper logo assets for the ALXV Labs and Folloup logos
- generated e-paper icon assets for all PNG files currently in `assets/icons/`
- empty generated footer-icon scaffolding ready for future manifest entries
- `project_assets::GetLogo(...)`, `GetIcon(...)`, and `GetFooterIcon(...)`
  lookup helpers by generated enum IDs

Keep generated files reproducible from `assets/` and `scripts/`. Do not hand
edit generated asset source files. See `docs/asset-generation.md`.

### `components/design_tokens`

This header-only component owns shared product UI constants. It is intentionally
named `design_tokens`, not e-paper design tokens, because the values describe
the Folloup UI language rather than the SSD1677 display driver.

Current scope:

- spacing scale
- a canonical four-step e-paper grayscale ramp (`gray1` through `gray4`) plus
  semantic grayscale color roles
- typography roles, sizes, and weights
- common component sizing constants used by the e-paper UI being ported

Use this component as the first dependency when porting small pieces from the
old `epaper_lib`. Keep UI tokens independent from display hardware and
framebuffer mechanics.

### `components/epaper_ui`

This component owns reusable e-paper UI primitives that render into the app's
portrait framebuffer. It depends on `design_tokens` for visual constants and
`project_assets` for embedded icons/logos, but it does not depend on app
services or startup/runtime policy.

Current scope:

- generated Inter bitmap fonts used by e-paper UI typography roles
- a small role-aware bitmap font renderer
- the app status-bar state contract and renderer
- a reusable lock-screen renderer plus its dedicated state contract

App-owned runtime helpers in `main/` may compose service state into these UI
contracts, but the drawing primitives themselves should stay reusable and
service-agnostic.

### UI Layering

The e-paper UI stack is now intentionally split across three layers:

- `design_tokens` owns product-wide spacing, grayscale, typography, and
  component metrics.
- `epaper_ui` owns reusable presentation primitives such as bitmap fonts,
  role-aware text rendering, the status bar renderer, the lock-screen
  renderer, and future view widgets ported from `followup`.
- app-owned runtime helpers in `main/` compose service state into UI contracts.
  Today that includes `status_bar_runtime`, which translates
  `power_service`, `wifi_service`, `timezone_service`, and sleep/shutdown state
  into a neutral `epaper_ui::StatusBarState`, and `lock_screen_runtime`, which
  composes time plus status indicators into `epaper_ui::LockScreenState` and
  lock-screen visibility.

`display_service` remains the owner of the physical panel, framebuffer, refresh
mode decisions, and sleep/wake transitions. It may consume `epaper_ui`
renderers, but it should not become the home for product state composition or a
grab bag of reusable widgets.

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

`main/status_bar_runtime.cpp` is an example of the intended app-runtime helper
pattern. It is not a reusable component and does not own hardware or rendering.
Its job is to compose product state into UI-facing data contracts that
`display_service` can render through `epaper_ui`.

The current early startup sequence is:

- Detects whether the running image is `ESP_OTA_IMG_PENDING_VERIFY`.
- Marks the image valid with `esp_ota_mark_app_valid_cancel_rollback()`.
- Asserts the Sticky power latch before OTA validation.
- Initializes `power_service`.
- Logs one power/battery diagnostic snapshot.
- Initializes `feedback_service` and requests the startup feedback.
- Initializes `storage_service` and logs one MicroSD diagnostic snapshot.
  On this board, when a card is present, storage must initialize before the
  shared-bus display path so the card enters SPI mode first and remains mounted.
- Initializes `display_service` and clears the e-paper panel to a blank screen.
- Initializes `touch_service` and logs app-facing touch events.
- Initializes `imu_service` and logs three direct IMU samples for bring-up.
- Initializes `environment_service` and logs three direct SHT40 samples for
  bring-up.
- Starts the auto-sleep runtime, which wires `device_sleep_service`, polls IMU
  samples for inactivity, owns the auto-sleep worker task, and handles display
  sleep/light sleep actions.
- Initializes `timezone_service`, which loads timezone/time-sync state from
  NVS, applies the configured timezone, and seeds system time from the PCF8563
  RTC when available.
- Initializes `wifi_service`, which loads saved Wi-Fi credentials or built-in
  sdkconfig credentials, starts station mode when credentials exist, or starts
  AP setup mode when no credentials are available.
- Initializes `recording_service` and logs recording status.
- Initializes `button_service`.
- Subscribes to button and touch events, forwards user activity into
  auto-sleep, forwards interaction feedback into `feedback_service`, and handles
  power-button shutdown intents.
- Subscribes to Wi-Fi events and forwards connection state into
  `timezone_service` so network time sync starts after station connectivity is
  available.
- Runs a small shutdown task so button callbacks can request shutdown without
  directly executing the power-latch release sequence.

Power-button long press start arms shutdown, and long-press release requests it.
This avoids releasing the latch while the physical power button is still being
held. The shutdown task also waits briefly after release before calling
`power_service::RequestShutdown()` so the analog button/Q2 bootstrap path has
time to stop feeding `PWR_EN`. The button callback only notifies the AppShell
shutdown task for shutdown requests; the task calls the power service so
latch-release timing does not run inside the button callback.

## Task Mapping

App-owned FreeRTOS tasks use the shared mapping in
`components/task_config/include/followup_task_config.h`. The app is optimized
around a simple split:

- CPU0 is the system/network side. ESP-IDF already runs the main task,
  `esp_timer`, and Wi-Fi driver work there in the current `sdkconfig`, so app
  Wi-Fi/time coordination stays close to that side.
- CPU1 is the product hardware/UI side. Touch, audio capture, storage work,
  buzzer feedback, and sleep-driven display transitions are kept away from CPU0
  as the app scales.

On single-core builds, the shared task config maps the app core back to CPU0.

| Task | Owner | Priority | Core | Responsibility |
| --- | --- | ---: | --- | --- |
| `record_capture` | `recording_service` | 5 | CPU1 | Timing-sensitive microphone capture, pre-roll, and clip buffering. |
| `touch_service` | `touch_service` | 5 | CPU1 | GT911 interrupt servicing and app-facing touch events. |
| `app_sleep` | `device_sleep_runtime` | 4 | CPU1 | Display sleep, light-sleep entry/exit, and wake recovery actions. |
| `app_shutdown` | `app_shell` | 4 | CPU1 | Deferred power-latch release after POWER_OK long-press release. |
| `sleep_motion` | `device_sleep_runtime` | 3 | CPU1 | 200 ms IMU polling and motion/stillness classification. |
| `wifi_transition` | `wifi_service` | 3 | CPU0 | Wi-Fi station/AP/stop/disconnect transitions. |
| `wifi_callbacks` | `wifi_service` | 3 | CPU0 | App-facing Wi-Fi event delivery outside ESP event callbacks. |
| `storage_service` | `storage_service` | 2 | CPU1 | Long-running SD operations such as format. |
| `timezone_sync` | `timezone_service` | 2 | CPU0 | SNTP sync, system-time update, and RTC writeback. |
| `buzzer` | `buzzer_service` | 2 | CPU1 | Non-critical PWM tone and pattern playback. |

The mapping intentionally keeps long-running SD work below input and audio
capture. Future tasks should be added to `task_config` first, with a short
ownership rationale, rather than using local priority/core literals.

Driver-specific wiring should stay out of `main/`; app startup should call
service-level APIs instead. Add product-specific sequencing in `app_shell`, not
inside reusable components.

Wi-Fi and time services follow the same boundary:

- `wifi_service` owns `esp_netif`, the default ESP event-loop registration,
  `esp_wifi` mode changes, station/AP configuration, NVS credential storage,
  network scans, and the HTTP backend server used during AP setup.
- `timezone_service` owns timezone catalog/aliases, persisted timezone settings,
  SNTP setup, system-time updates, PCF8563 RTC read/write through
  `power_service`, and backend HTTP routes for time settings.
- `app_shell` wires the two services together by forwarding Wi-Fi connectivity
  events into `timezone_service::SetNetworkConnected(...)`.

Runtime-persisted settings live in service-owned NVS namespaces:

- `wifi`: `ssid`, `password`
- `timezone`: `enabled`, `tz_name`, `location`, `time_src`, `ntp_sync`,
  `ntp_epoch`

The build-time Wi-Fi/time defaults live under `Folloup Settings`:

- `CONFIG_FOLLOWUP_WIFI_AP_PREFIX`
- `CONFIG_FOLLOWUP_WIFI_STA_SSID`
- `CONFIG_FOLLOWUP_WIFI_STA_PASSWORD`
- `CONFIG_FOLLOWUP_WIFI_START_IN_AP_MODE`
- `CONFIG_FOLLOWUP_TIME_SYNC_DEFAULT_ENABLED`
- `CONFIG_FOLLOWUP_DEFAULT_TIMEZONE_NAME`

Saved NVS Wi-Fi credentials take precedence over built-in sdkconfig
credentials. If neither exists, or if `CONFIG_FOLLOWUP_WIFI_START_IN_AP_MODE`
is enabled, `wifi_service` enters open AP setup mode and serves backend routes
at the SoftAP URL, normally `http://192.168.4.1`. The current backend
intentionally exposes JSON/form endpoints only; it does not embed the old
portal UI and does not add DNS captive-portal redirection.

Current Wi-Fi backend routes:

- `GET /`
- `GET /api/status`
- `GET /api/scan`
- `POST /api/configure`
- `POST /api/disconnect`

Current time backend routes registered on the same HTTP server:

- `GET /api/settings/time`
- `PATCH /api/settings/time`
- `GET /api/runtime/time`
- `GET /api/timezone/list`

Auto-sleep is split across a policy component and a product runtime helper:
`device_sleep_service` owns sleep state, timers, timeout validation, blocker
state, and transition events, but it does not touch display, GPIO, or ESP sleep
hardware. `main/device_sleep_runtime.cpp` owns product-specific auto-sleep
runtime behavior: IMU inactivity polling, the event worker task, display sleep
commands, ESP light-sleep entry, POWER_OK wake handling, and app-level blocker
aggregation. `app_shell` should only provide settings, provide app-owned
signals such as shutdown-pending state, start the runtime, and forward user
activity. See `docs/auto-sleep.md` for the stable feature behavior and the
deferred FIFO/shared-interrupt plan.

Current auto-sleep behavior:

- `main/device_sleep_runtime.cpp` polls `imu_service::ReadSample(...)` every
  `200 ms` and converts acceleration deltas from `g` to `mg`.
- Motion is detected when the axis-delta sum is at least `60 mg` or the largest
  axis delta is at least `25 mg`.
- Stillness is detected only after a continuous `2 s` window where the
  axis-delta sum is at most `20 mg` and the largest axis delta is at most
  `8 mg`.
- Display sleep refreshes the e-paper panel to a blank screen and then puts the
  panel to sleep.
- ESP32-S3 light sleep first configures `PWR_HOLD` / `GPIO45` and `PWR_LOCK` /
  `GPIO46` to remain driven high during light sleep, waits for `POWER_OK` /
  `GPIO4` to be released, and arms `POWER_OK` through EXT1 as the active-low
  wake source. It also arms wake-only `POWER_OK` event suppression before
  entering ESP light sleep so the wake press cannot become a normal long-press
  shutdown request. It then refreshes the panel to a blank screen, puts the
  panel to sleep, and enters
  `esp_light_sleep_start()`.
- The wake-causing power-button events are consumed as wake-only after light
  sleep, so they do not trigger normal power-button behavior or leave
  `shutdown_pending` set as an auto-sleep blocker.
- After light-sleep wake, the display is restored to a blank screen with a
  forced full refresh and `touch_service` recovers the GT911 controller before
  normal touch input resumes.
- Inactivity is blocked while recording is active, armed, saving, or exporting;
  while shutdown is pending; while an e-paper refresh is active; during
  app-declared storage write activity; while AP setup mode is active; and while
  SNTP time sync is in progress.
- The current bench defaults are `10 s` for display sleep and `30 s` for light
  sleep. Production defaults should be raised later when product behavior is no
  longer being tuned on the bench.

SD-card formatting remains exposed through `storage_service`, but no demo
button path currently invokes it. A future app UI should call the storage API
through its own action/controller layer.

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
- PDM microphone clock: `GPIO_NUM_19`
- PDM microphone data: `GPIO_NUM_20`
- PDM microphone power enable: `GPIO_NUM_38`, active high
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
- touch power enable: `GPIO_NUM_42`
- touch interrupt: `GPIO_NUM_21`
- touch reset: `GPIO_NUM_41`
- touch I2C bus port: `I2C_NUM_0`
- touch I2C SCL: `GPIO_NUM_2`
- touch I2C SDA: `GPIO_NUM_3`
- charger enable: `GPIO_NUM_39`, active low
- charger state: `GPIO_NUM_40`
- power-input ADC sense: `GPIO_NUM_9`
- sensor I2C bus port: `I2C_NUM_1`
- sensor I2C SCL: `GPIO_NUM_0`
- sensor I2C SDA: `GPIO_NUM_1`
- BQ27220 I2C address: `0x55`
- BQ27220 interrupt pin: `GPIO_NUM_7`
- PCF8563 I2C address: `0x51`
- LSM6DS3TR-C I2C address: `0x6A`
- IMU interrupt pin: `GPIO_NUM_7`
- SHT40 primary I2C address: `0x44`
- SHT40 alternate I2C address: `0x45`
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
- `sticky_board::EnableTouchPower()`
- `sticky_board::ConfigureTouchInterruptPin(...)`
- `sticky_board::ReadTouchInterruptLevel(...)`
- `sticky_board::EnsureSensorI2cBus(...)`
- `sticky_board::CreateSensorI2cBus(...)`
- `sticky_board::CreateTouchI2cBus(...)`
- `sticky_board::AddBq27220Device(...)`
- `sticky_board::AddPcf8563Device(...)`
- `sticky_board::AddLsm6ds3Device(...)`
- `sticky_board::AddSht40Device(...)`

Keep this layer focused on raw board mechanics: pins, buses, GPIO polarity, ADC
setup, and latch timing.

The sensor I2C bus is shared by the BQ27220, PCF8563, LSM6DS3TR-C, and SHT40.
New callers should use `sticky_board::EnsureSensorI2cBus(...)` instead of
creating their own bus handle. This keeps the ESP-IDF bus object singleton-like
while allowing each service to add its own device handle.

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

The auto-sleep runtime preserves `PWR_HOLD` / `GPIO45` and `PWR_LOCK` /
`GPIO46` as driven-high outputs during ESP light sleep, then arms `POWER_OK` /
`GPIO4` through EXT1 as the wake source. The managed button component still owns
normal awake-state debounce and event generation; light-sleep wake setup stays
in `main/device_sleep_runtime.cpp` so pre-sleep wake-only power-button event
suppression and immediate display/touch recovery remain part of the auto-sleep
policy.

### `components/buzzer_service`

This C++ component owns low-level buzzer playback. It uses ESP-IDF LEDC PWM on
the Sticky buzzer pin and hides timer/channel/duty details from app-facing
services.

Current scope:

- `BUZZER_PWM` on `GPIO48`
- LEDC low-speed mode
- LEDC timer 0 and channel 0
- 10-bit duty resolution
- asynchronous command queue and worker task
- `PlayTone(...)`, `PlayPattern(...)`, and `Stop()`
- named startup, click, long-click, double-click, error, and shutdown patterns

The service drives tones at a 50 percent PWM duty cycle, which is the loudest
useful square-wave drive for this passive PWM buzzer. A 100 percent duty cycle
would be DC and would not produce the intended tone.

App-facing code should normally call `feedback_service`, not this component
directly. This keeps product feedback policy separate from PWM details.

### `components/feedback_service`

This C++ component owns app-facing haptic/audio feedback policy. It maps product
events onto buzzer patterns without exposing buzzer hardware details to
`app_shell`.

Current scope:

- initializes `buzzer_service`
- maps startup, button click, button double-click, button long-press, touch
  contact, shutdown, and error feedback onto buzzer patterns
- keeps app-level feedback names separate from low-level tone/pattern names

AppShell requests feedback events for button single-click, double-click,
long-press-start, touch contact, startup, and shutdown. It should not know about
LEDC timer numbers, PWM duty values, GPIO setup, or exact buzzer pattern
composition.

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
the `sd_card` wrapper and owns app SD-card mount and format state.

Current scope:

- use the schematic page 5 MicroSD pin map
- check `SD_DETECT`
- mount `/sdcard` during boot when a card is present
- keep the card mounted during normal runtime after successful boot-time init
- format the SD card on request, set the `FOLLOUP` volume label, and recreate
  the source-app directory layout:
  `/recordings`, `/todos`, `/summaries`, `/files`, `/trash`,
  `/trash/recordings`, and `/trash/todos`
- log mount status, total/free bytes, and a small root directory preview
- write/read `/sdcard/SDPROBE.TXT` once as a bring-up probe

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

Hardware validation on Sticky showed an extra board-specific constraint: when an
SD card is inserted, the card must be initialized on the shared SPI bus before
the e-paper panel starts using that bus, and the card should remain mounted
afterward. Tearing the card back down after boot caused the panel to log a
refresh without visibly updating the screen. Treat "SD first, then display, and
keep SD mounted" as a required startup policy on this hardware revision.

### `components/pdm_mic`

This is the input-only PDM microphone driver adapted from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo 7.38.00 AM/components/pdm_mic
```

The source demo modeled the microphone as an `AudioCodec`, but Sticky has no
speaker/playback path in current product scope. This port intentionally keeps
only the ESP-IDF I2S PDM RX side and omits output volume, mute, TX channels,
duplex behavior, and playback state.

Current scope:

- initialize ESP-IDF I2S PDM RX with 16 kHz, mono, signed 16-bit PCM output
- use bounded `i2s_channel_read(...)` calls for PCM capture
- leave recording policy, buffering, voice activity, and file output to
  higher-level services

### `components/microphone_service`

This is the app-facing microphone hardware layer. It composes Sticky board pin
definitions with the input-only `pdm_mic` driver.

Current scope:

- configure `PDM_CLK` on `GPIO19` and `PDM_DATA` on `GPIO20`
- control microphone power with `PDM_EN` on `GPIO38`, active high
- initialize, enable, disable, and read PCM samples from the PDM mic driver
- expose the 16 kHz sample rate used by voice recording
- calculate and retain a simple input-level percentage from captured PCM chunks
- keep recording policy, pre-roll, clip ownership, VAD, and WAV output out of
  the microphone hardware layer

### `components/recording_service`

This is the app-facing voice-input recording layer. It composes the input-only
`microphone_service` with app policy for pre-roll, recording state, clip
ownership, input-level telemetry, and WAV file output.

Current scope:

- create a dedicated capture task that reads short PCM chunks from
  `microphone_service`
- keep a one-second PSRAM-backed pre-roll ring buffer while armed
- support starting a recording with or without pre-roll
- store the active clip in PSRAM-backed chunks with a 10-second max duration
- track a simple input-level percentage for UI/debug/VAD preparation
- expose `Arm()`, `Start()`, `Finish()`, `Cancel()`, `DiscardClip()`, and
  `GetRecordedClip()`
- save the latest clip as a mono 16-bit PCM WAV file on MicroSD

The service does not implement playback. Future voice-product work should build
VAD, upload/transcription, and display status on top of this service rather
than adding those policies to `pdm_mic`.

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
- render the startup splash with `RefreshFullBase()`
- own the current portrait framebuffer surface and its refresh policy
- render the current active screen, including the home bridge screen or lock
  screen, together with the appropriate UI chrome
- enter panel sleep without a special transitional text screen
- restore the current screen with a forced full refresh after display wake or
  light-sleep recovery
- log panel refresh metrics and expose refresh-in-progress state for
  auto-sleep blocking

Current UI state:

- `display_service` now owns a minimal screen model with the temporary home
  demo/bridge surface plus a real lock screen
- the status bar is now rendered through `epaper_ui`
- the lock screen uses its own `epaper_ui` renderer and a dedicated runtime
  helper in `main/lock_screen_runtime.cpp`
- sleep and shutdown indicators are driven through `status_bar_runtime`
  immediately before display sleep, light sleep, and deep-sleep shutdown
  transitions

The long-term direction is to replace the remaining demo bridge with real view
renderers and rename the display API away from `DemoSelection` toward a
view-oriented model. Track that work in `docs/display-demo-cleanup.md`.

Because the SSD1677 path shares `SPI2_HOST` with MicroSD, `display_service`
depends on `storage_service` having already performed SD bring-up when a card is
inserted. The display path should not reorder itself ahead of storage during
boot on this board.

`display_service` owns app-facing display policy. Driver-specific wiring and
SSD1677 commands must stay out of `main`. Raw board pin ownership stays in
`board`, and low-level SSD1677 command sequencing stays in `epaper_panel`.

### `components/gt911`

This is the generic GT911 capacitive touch driver ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo 7.38.00 AM/components/gt911
```

The driver should stay board-agnostic. It receives an initialized
`i2c_master_bus_handle_t`, the GT911 interrupt/reset pins, and the logical
coordinate size from its caller. It should not own Sticky-specific GPIO numbers,
I2C ports, power-enable behavior, or app policy.

Current scope:

- select and probe GT911 address `0x14` / `0x5D`
- perform the Goodix reset sequence, including the post-reset INT-low sync pulse
  needed for reliable scan startup
- read product ID and sensor resolution
- read up to five touch points from `0x8150`
- clear the status register after each ready report
- map raw sensor coordinates into caller-provided logical dimensions
- expose a simple callback/polling API

For bring-up, a readable product ID is not sufficient proof that touch is
working. If the resolution reads as zero or remains at the driver's fallback
`2048x2048`, inspect the reset/INT sync sequence before chasing unrelated
peripherals or config blobs. See `docs/gt911-touch-reset-debugging.md`.

Important: the post-reset INT-low sync pulse in `components/gt911/gt911.cpp`
must be preserved. It was added from the debugging work documented in
`docs/gt911-touch-reset-debugging.md`: without that pulse, this board's GT911
can respond to I2C product-ID reads while failing to report the real `480 x 800`
resolution or any usable touch points.

### `components/touch_service`

This is the app-facing touch layer. It composes `board` pin definitions, touch
power control, the dedicated touch I2C bus, the GT911 driver, and the
`TOUCH_INT` interrupt.

Current scope:

- enable `TP_PWR_EN`
- initialize the dedicated touch I2C bus on `I2C_NUM_0`
- initialize GT911 with logical portrait coordinates `480 x 800`
- recover GT911 after ESP light sleep by resetting/reinitializing the controller
  before normal touch input resumes
- attach a negative-edge ISR to `TP_INT`
- wake a touch worker task from the ISR
- service the GT911 outside interrupt context
- log bring-up details, interrupt servicing, and mapped touch points
- expose a typed event callback API for app-level routing in `app_shell`

The light-sleep recovery path is required on this board. After ESP light sleep,
the GT911 can stop reporting touches unless the service runs the reset/begin
sequence again and reattaches `TP_INT`. The GPIO ISR service itself is global
and may already be installed; recovery should handle that as an already-ready
state instead of logging it as an error.

The service intentionally does not draw directly to e-paper. Touch gestures and
points are app intents; display drawing remains owned by `display_service`.

### `components/lsm6ds3`

This is the generic LSM6DS3 / LSM6DS3TR-C inertial sensor driver ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo 7.38.00 AM/components/lsm6ds3
```

The driver should stay board-agnostic. It receives an initialized
`i2c_master_dev_handle_t` or `spi_device_handle_t` from its caller and exposes
register reads, register writes, accelerometer, gyro, temperature, and FIFO
helpers. It should not own Sticky-specific GPIO numbers, I2C ports, interrupt
policy, sleep policy, or app-level inactivity decisions.

Current scope:

- probe WHO_AM_I register `0x0F`
- accept `0x6A` for LSM6DS3TR-C and `0x69` for LSM6DS3
- configure direct accelerometer and gyro sampling for bring-up
- expose temperature, acceleration, gyro, and FIFO helper APIs from the source
  driver

### `components/imu_service`

This is the app-facing IMU layer. It composes `board` sensor-bus helpers with
the generic LSM6DS3 driver.

Current scope:

- initialize the shared sensor I2C bus on `I2C_NUM_1`
- add the LSM6DS3TR-C at address `0x6A`
- verify WHO_AM_I before marking the service initialized
- configure the same first-pass settings as the source demo: accelerometer
  `4g` / `104Hz` / `100Hz BW`, gyro `245dps` / `104Hz` / `100Hz BW`
- log three direct sample reads at startup for hardware validation
- expose `imu_service::ReadSample(...)` for direct temperature, acceleration,
  and gyro reads

The first port intentionally does not enable FIFO streaming or claim the IMU
interrupt on `GPIO7`. FIFO and interrupt-driven wake/sleep policy should be
added only after direct samples are verified on hardware and the shared
`GPIO7` ownership with the BQ27220 interrupt path is designed explicitly.

The current auto-sleep implementation uses direct 200 ms polling through
`imu_service::ReadSample(...)`. Hardware validation showed this is responsive
enough for pickup/display wake behavior, so FIFO-backed sampling and IMU
interrupt handling are deferred. The IMU service must not attach a `GPIO7` ISR
or configure IMU interrupt routing until a measured power or responsiveness
problem justifies that optimization.

If a future milestone enables the IMU interrupt path, `GPIO7` must be owned by
one shared-line runtime instead of by `power_service` or `imu_service`
independently. That owner should keep the ISR minimal, defer all I2C work to a
task, preserve the existing BQ27220 `BFG_INT` level/status diagnostics, and then
query the IMU interrupt status or FIFO state. Logs should identify which source
asserted the shared line so battery diagnostics and inactivity wake behavior
remain debuggable together.

### `components/sht40`

This is the generic SHT40 temperature/humidity sensor driver ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo 7.38.00 AM/components/sht40
```

The driver should stay board-agnostic. It receives an initialized
`i2c_master_dev_handle_t` from its caller and exposes soft reset, serial-number
read, and temperature/humidity measurement helpers. It should not own
Sticky-specific GPIO numbers, I2C ports, address fallback policy, or app-level
environment display/logging policy.

Current scope:

- soft reset with command `0x94`
- read serial number with command `0x89`
- read high/medium/low precision temperature and humidity measurements
- validate SHT40 CRC bytes before accepting serial or measurement data
- convert raw values to degrees Celsius and relative humidity percent

### `components/environment_service`

This is the app-facing ambient environment layer. It composes `board`
sensor-bus helpers with the generic SHT40 driver.

Current scope:

- initialize the shared sensor I2C bus on `I2C_NUM_1`
- try SHT40 primary address `0x44`
- fall back to SHT40 alternate address `0x45` when the primary probe fails
- read the sensor serial number before marking the service initialized
- reset the shared sensor I2C bus after failed SHT40 serial/measurement
  transactions so a bad probe does not poison later shared-bus users
- log three high-precision temperature/humidity samples at startup for hardware
  validation
- expose `environment_service::ReadSample(...)` for direct app-facing reads

The first port intentionally does not create a background sampling task or draw
environment values to e-paper. Product policy for sampling cadence, smoothing,
weather UI, and persistence should be layered above this service later.

## Hardware Notes

- Main controller: `ESP32-S3R8`.
- External flash: 256 Mbit / 32 MB QSPI flash.
- PSRAM: 8 MB octal PSRAM.
- BQ27220 address: `0x55`.
- PCF8563 address: `0x51`.
- LSM6DS3TR-C address: `0x6A`.
- SHT40 address: primary `0x44`, fallback `0x45`.
- Buzzer PWM output: `GPIO48`.
- PDM microphone uses `PDM_CLK` on `GPIO19`, `PDM_DATA` on `GPIO20`, and
  `PDM_EN` on `GPIO38`.
- MicroSD uses SDSPI mode only: `SD_CLK/SCK` on `GPIO13`, `SD_CMD/MOSI` on
  `GPIO14`, `SD_D0/MISO` on `GPIO12`, `SD_D3/CS` on `GPIO8`, `SD_PWR_EN` on
  `GPIO10`, and `SD_DETECT` on `GPIO11`. `SD_D1` and `SD_D2` are not connected.
- The SSD1677 e-paper panel shares `SPI2_HOST` with MicroSD: `EP_SCK` on
  `GPIO13`, `EP_SDI/MOSI` on `GPIO14`, `EP_SDO/MISO` on `GPIO12`, `EP_CS` on
  `GPIO15`, `EP_DC` on `GPIO16`, `EP_RST` on `GPIO17`, `EP_BUSY` on `GPIO18`,
  and `EP_PWR_EN` on `GPIO47`.
- Hardware bring-up confirmed a board-specific shared-bus rule: with an SD card
  inserted, the firmware must initialize MicroSD first and keep it mounted
  before bringing up the e-paper panel. If the card is removed, display init
  behaves normally without that constraint.
- The e-paper panel is 800 x 480 raw landscape pixels. The bring-up
  `display_service` draws portrait content by mapping logical 480 x 800
  coordinates into the raw SSD1677 framebuffer.
- GT911 touch uses a separate I2C bus: `TP_I2C_SCL` on `GPIO2`,
  `TP_I2C_SDA` on `GPIO3`, `TP_PWR_EN` on `GPIO42`, `TP_INT` on `GPIO21`, and
  `TP_RSTn` on `GPIO41`. The app-facing touch coordinate space is currently
  logical portrait `480 x 800` to match `display_service`.
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
- BQ27220, PCF8563, LSM6DS3TR-C, and SHT40 share the sensor I2C bus.
- Sensor I2C uses `GPIO0` for SCL and `GPIO1` for SDA.
- `GPIO0` is also an ESP32-S3 boot strapping/download pin. Create the sensor
  I2C bus only after boot has completed and startup pin levels are no longer
  part of the boot-mode decision.
- `GPIO7` is shared by the BQ27220 interrupt line and the IMU interrupt path.
  The first IMU port does not attach an ISR or configure IMU interrupt routing.
  Current auto-sleep behavior intentionally keeps using IMU polling. Future
  inactivity/sleep work must coordinate this shared line through one deferred
  interrupt owner instead of letting either service claim GPIO7 independently.

## Configuration

Configuration is file-based and should stay reproducible:

- `sdkconfig.defaults` captures the intended project defaults.
- `sdkconfig` captures the resolved ESP-IDF configuration.
- `partitions.csv` defines the OTA partition table.

Project-specific Kconfig options live under `Folloup Settings`. Auto-sleep
currently exposes reproducible build-time defaults for display sleep and light
sleep timeout seconds; `0` disables the corresponding stage, and a nonzero light
sleep timeout must be greater than or equal to the display sleep timeout.

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
  -> feedback_service
       -> buzzer_service -> board -> ESP-IDF LEDC driver
  -> device_sleep_service
  -> storage_service
       -> board
       -> sd_card -> ESP-IDF SDSPI/FATFS/SDMMC drivers
  -> recording_service
       -> microphone_service
            -> board
            -> pdm_mic -> ESP-IDF I2S/GPIO drivers
  -> display_service
       -> board
       -> epaper_panel -> ESP-IDF SPI/GPIO drivers
  -> touch_service
       -> board
       -> gt911 -> ESP-IDF I2C/GPIO drivers
  -> imu_service
       -> board
       -> lsm6ds3 -> ESP-IDF I2C/SPI drivers
  -> environment_service
       -> board
       -> sht40 -> ESP-IDF I2C driver
```

Avoid making `bq27220`, `pcf8563`, `sd_card`, `pdm_mic`, `epaper_panel`,
`gt911`, `lsm6ds3`, or `sht40` depend on `board`; that would make generic
drivers board-specific. `microphone_service` is allowed to depend on `board`
because it is the Sticky-specific app-facing microphone layer.
