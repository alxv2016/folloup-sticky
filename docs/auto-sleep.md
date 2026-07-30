# Auto Sleep

Auto sleep uses the QMI8658 IMU to detect inactivity, enters e-paper display
sleep first, and later enters ESP32-S3 light sleep. The current implementation is
proven on this hardware and intentionally uses direct IMU polling
instead of FIFO or IMU interrupts.

## Runtime Ownership

Auto sleep is split between policy and hardware runtime code:

- `device_sleep_service` owns the sleep state machine, inactivity timers,
  timeout validation, blocker state, and transition events.
- `main/device_sleep_runtime.cpp` owns product-specific hardware behavior:
  IMU polling, display sleep commands, ESP light-sleep entry, `ACTION` wake
  setup and blocker aggregation.
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
`ACTION` / `GPIO0` or the PMIC interrupt wakes the ESP32-S3 from `light_sleeping`.

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

There is no power latch to protect on this board -- the AXP2101 holds the rails
across light sleep on its own, which removes the Sticky's whole `PWR_HOLD` /
`PWR_LOCK` sleep-GPIO problem.

One protection does carry over: `ACTION` / `GPIO0` is both a light-sleep wake
source and the app's record button. The runtime arms wake-only suppression before
calling `esp_light_sleep_start()` so the wake-causing press cannot leak into
`app_shell` and arm a recording. The suppression is cleared by the matching
release/click event after wake, or by a timeout if that event never arrives.

The AXP2101 interrupt line (`GPIO38`) is the second wake source, which is how a
`PWR` press wakes the board.

The light-sleep sequence is:

1. Configure `ACTION` / `GPIO0` as an input with pull-up.
2. Wait for `ACTION` / `GPIO0` to be released/high.
3. Arm `ACTION` / `GPIO0` and the PMIC IRQ / `GPIO38` as active-low
   `gpio_wakeup_enable` light-sleep wake sources (not EXT1: this board uses the
   light-sleep GPIO-wake path, which leaves the pads on the digital peripheral).
4. Suspend the button-service polling timer, so light sleep's clock jump cannot
   replay a burst of missed ticks and destroy click classification on wake.
5. Arm wake-only `ACTION` event suppression.
6. Refresh the e-paper panel to a blank screen.
7. Wait for the e-paper refresh to finish and put the panel into sleep.
8. Call `esp_light_sleep_start()`.
9. On wake, disarm the GPIO wake sources, restore the `ACTION` pad, and resume
   button polling before anything slow runs.
10. Commit the wake transition immediately, without queueing a second wake event.
11. Consume the wake-causing power-button event as wake-only.
12. Restore the display with a forced full refresh, even if software state has
    already moved back to awake.

Normal awake-state power-button interactions remain available outside the
light-sleep wake path. Today that means a short press of the
`PWR` key toggles the lock screen, while a ~1s `PWR` hold opens the shutdown
confirmation. Both arrive as AXP2101 interrupts rather than GPIO button events
chord and then requires explicit confirmation through the global shutdown
modal.

## Sleep Blockers

Auto sleep is blocked during workflows where sleeping would interrupt active
work or make hardware state harder to reason about.

Current blockers:

- recording active
- recording armed
- recording saving or exporting
- shutdown pending, including the shutdown confirmation modal
- display refresh active
- app-declared storage write activity
- Wi-Fi access-point setup mode
- SNTP time sync in progress

Plain USB power does not block auto sleep.

During SD format, `storage_service::IsWriteBusy()` raises the `storage_write`
blocker. That keeps the sleep state machine from entering display sleep or
light sleep in the middle of the format operation. IMU motion polling still
continues during that time, but it reads the QMI8658 over the shared sensor
I2C bus and does not directly contend with the shared SPI bus used by MicroSD
and the e-paper panel. Motion logs during formatting are therefore expected and
are not, by themselves, evidence that the SD format path is being interrupted.

## Configuration

The build-time settings live under `Folloup Settings`:

- `CONFIG_FOLLOWUP_AUTO_SLEEP_DISPLAY_SLEEP_TIMEOUT_SECONDS`
- `CONFIG_FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS`

Current defaults:

- display sleep: `180 s` (3 minutes)
- light sleep: `1800 s` (30 minutes)

Set either timeout to `0` to disable that stage. When both stages are enabled,
the light-sleep timeout must be greater than or equal to the display-sleep
timeout.

## Logging

The runtime logs the resolved auto-sleep settings at startup, motion and
no-motion detection, blocker changes, stage transitions, display sleep/wake
actions, light-sleep entry, and light-sleep wake cause after
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

The QMI8658 IMU shares the sensor I2C bus with the PMIC and RTC. If the IMU
interrupt path is added later, neither `power_service` nor `imu_service` should
claim `GPIO7` independently. Add one shared-line owner that:

- owns the `GPIO7` ISR
- keeps the ISR minimal
- defers all I2C work to a task
- checks and logs PMIC interrupt state without losing existing diagnostics
- checks and logs IMU interrupt source or FIFO state
- identifies which source asserted the shared line
- preserves current auto-sleep behavior until the new interrupt path is proven
