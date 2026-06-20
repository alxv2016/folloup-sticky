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
- `display_sleeping`: the e-paper panel has refreshed to a blank screen and
  then entered panel sleep.
- `light_sleeping`: the e-paper panel has refreshed to a blank screen, entered
  panel sleep, and the ESP32-S3 has entered `esp_light_sleep_start()`.

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

1. Refresh the e-paper panel to a blank screen.
2. Wait for the e-paper refresh to finish.
3. Put the e-paper panel into sleep.
4. Leave the panel asleep until motion or user interaction wakes it.

Motion or user interaction wakes the display and restores the blank app surface
with a full refresh.

## Light Sleep

After the configured light-sleep timeout has elapsed during the same no-motion
period, the runtime enters ESP32-S3 light sleep.

The Sticky power path needs two board-specific protections during light sleep:

- `PWR_HOLD` / `GPIO45` and `PWR_LOCK` / `GPIO46` must remain driven high while
  the ESP32-S3 sleeps. Without an explicit sleep GPIO configuration, ESP-IDF's
  automatic sleep GPIO handling can let the latch pins stop holding the board in
  the same powered state. USB power can mask this on the bench, but battery-only
  light sleep depends on the latch pins staying asserted.
- `POWER_OK` / `GPIO4` is both the active-low light-sleep wake source and the
  normal app power button. The runtime arms wake-only suppression before calling
  `esp_light_sleep_start()` so the wake-causing press cannot leak into
  `app_shell` as a normal long-press shutdown request. The suppression is
  cleared by the matching release/click event after wake.

The light-sleep sequence is:

1. Configure `PWR_HOLD` / `GPIO45` and `PWR_LOCK` / `GPIO46` to remain driven
   high during light sleep.
2. Configure `POWER_OK` / `GPIO4` as an input with pull-up.
3. Wait for `POWER_OK` / `GPIO4` to be released/high.
4. Arm `POWER_OK` / `GPIO4` through EXT1 as an active-low light-sleep wake source.
5. Arm wake-only `POWER_OK` event suppression.
6. Refresh the e-paper panel to a blank screen.
7. Wait for the e-paper refresh to finish and put the panel into sleep.
8. Call `esp_light_sleep_start()`.
9. On wake, disable the EXT1 wake source and restore GPIO4 to digital input mode.
10. Commit the wake transition immediately, without queueing a second wake event.
11. Consume the wake-causing power-button event as wake-only.
12. Restore the display with a forced full refresh, even if software state has
    already moved back to awake.
13. Recover the GT911 touch controller before normal touch input resumes.

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
- Wi-Fi access-point setup mode
- SNTP time sync in progress

Plain USB power does not block auto sleep.

## Configuration

The build-time settings live under `Folloup Settings`:

- `CONFIG_FOLLOWUP_AUTO_SLEEP_DISPLAY_SLEEP_TIMEOUT_SECONDS`
- `CONFIG_FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS`

Current bench defaults:

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
