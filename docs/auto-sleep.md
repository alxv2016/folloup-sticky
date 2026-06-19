# Auto Sleep

Auto sleep uses the LSM6DS3TR-C IMU to detect inactivity, enters e-paper display
sleep first, and later enters ESP32-S3 light sleep. The current implementation is
proven on reTerminal Sticky hardware and intentionally uses direct IMU polling
instead of FIFO or IMU interrupts.

## Runtime Ownership

Auto sleep is split between policy and hardware runtime code:

- `device_sleep_service` owns the sleep state machine, inactivity timers,
  timeout validation, blocker state, and transition events.
- `main/device_sleep_runtime.cpp` owns product-specific hardware behavior:
  IMU polling, display sleep commands, ESP light-sleep entry, `POWER_OK` wake
  setup, touch recovery after light sleep, and blocker aggregation.
- `app_shell` remains an orchestrator. It provides settings, forwards user
  activity, supplies app-owned blocker state, and starts the runtime.

## Stages

The device moves through three stages:

- `awake`: normal app behavior.
- `display_sleeping`: the e-paper panel has rendered `Display sleep` and then
  entered panel sleep.
- `light_sleeping`: the e-paper panel has rendered `Light sleep`, entered panel
  sleep, and the ESP32-S3 has entered `esp_light_sleep_start()`.

Motion or user interaction wakes the display from `display_sleeping`.
`POWER_OK` / `GPIO4` wakes the ESP32-S3 from `light_sleeping`.

## IMU Inactivity Detection

The runtime samples `imu_service::ReadSample(...)` every `200 ms` and compares
the latest accelerometer sample against the previous sample. Accelerometer
values are converted from `g` to `mg` before applying thresholds.

Current validated thresholds:

- Motion starts when the axis-delta sum is at least `60 mg`.
- Motion also starts when the largest single-axis delta is at least `25 mg`.
- Stillness requires the axis-delta sum to stay at or below `20 mg`.
- Stillness also requires the largest single-axis delta to stay at or below
  `8 mg`.
- No-motion is armed only after a continuous `2 s` stillness window.

These values were validated on-device. Normal table vibration did not require
threshold changes, and picking up the device wakes the display promptly.

## Display Sleep

After the configured display-sleep timeout has elapsed during a no-motion
period, the runtime asks `display_service` to enter display sleep.

The display sequence is:

1. Render the full-screen `Display sleep` message.
2. Wait for the e-paper refresh to finish.
3. Put the e-paper panel into sleep.
4. Suppress normal demo refreshes while the display is sleeping.

Motion or user interaction wakes the display and restores the normal demo screen
with a full refresh.

## Light Sleep

After the configured light-sleep timeout has elapsed during the same no-motion
period, the runtime enters ESP32-S3 light sleep.

The light-sleep sequence is:

1. Render the full-screen `Light sleep` message.
2. Wait for the e-paper refresh to finish.
3. Put the e-paper panel into sleep.
4. Arm `POWER_OK` / `GPIO4` as an active-low light-sleep wake source.
5. Call `esp_light_sleep_start()`.
6. On wake, disable the GPIO wake source.
7. Consume the wake-causing power-button event as wake-only.
8. Restore the display with a full refresh.
9. Recover the GT911 touch controller before normal touch input resumes.

GT911 recovery is required after ESP light sleep on this hardware. The touch
controller can remain unresponsive after the ESP32-S3 wakes unless
`touch_service` resets/reinitializes the GT911 and reattaches the `TP_INT`
handler. This is part of the light-sleep wake path, not an optional diagnostic
step.

Normal power-button long-press shutdown behavior remains available while the
device is awake.

## Sleep Blockers

Auto sleep is blocked during workflows where sleeping would interrupt active
work or make hardware state harder to reason about.

Current blockers:

- recording active
- recording armed
- recording saving or exporting
- shutdown pending
- display refresh active
- app-declared storage write activity

Plain USB power does not block auto sleep.

## Configuration

The build-time settings live under `Folloup Settings`:

- `CONFIG_FOLLOWUP_AUTO_SLEEP_DISPLAY_SLEEP_TIMEOUT_SECONDS`
- `CONFIG_FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS`

Current demo defaults:

- display sleep: `10 s`
- light sleep: `30 s`

Set either timeout to `0` to disable that stage. When both stages are enabled,
the light-sleep timeout must be greater than or equal to the display-sleep
timeout.

Suggested production defaults remain:

- display sleep: `180 s`
- light sleep: `1800 s`

## Logging

The runtime logs the resolved auto-sleep settings at startup, motion and
no-motion detection, blocker changes, stage transitions, display sleep/wake
actions, light-sleep entry, light-sleep wake cause, and touch recovery after
light sleep. These logs were used for on-device validation and should stay
stable enough for future hardware testing.

## Deferred FIFO And Shared ISR Plan

FIFO-backed sampling and IMU interrupt handling are intentionally deferred.
The current `200 ms` polling approach is simple, debuggable, responsive enough,
and does not require sharing `GPIO7` between two interrupt sources.

FIFO should be revisited only if one of these becomes true:

- polling consumes too much power
- short motion bursts are missed
- I2C traffic becomes a problem
- smoother motion history is needed for a future feature

IMU interrupt handling should be revisited only if one of these becomes true:

- motion wake needs to work from ESP light sleep
- pickup wake latency needs to be lower than the polling interval
- hardware measurements show polling should be replaced
- another feature needs IMU activity, inactivity, FIFO watermark, orientation,
  tap, or data-ready interrupts

`GPIO7` is shared by BQ27220 `BFG_INT` and the IMU interrupt path. If the IMU
interrupt path is added later, neither `power_service` nor `imu_service` should
claim `GPIO7` independently. Add one shared-line owner that:

- owns the `GPIO7` ISR
- keeps the ISR minimal
- defers all I2C work to a task
- checks and logs BQ27220 interrupt state without losing existing diagnostics
- checks and logs IMU interrupt source or FIFO state
- identifies which source asserted the shared line
- preserves current auto-sleep behavior until the new interrupt path is proven
