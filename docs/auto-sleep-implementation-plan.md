# Auto Sleep Implementation Plan

This plan ports the IMU-driven auto-sleep behavior from the local
`/Users/tieuvong/Development/followup` firmware into this repo.

The source behavior is staged:

- detect sustained no-motion from IMU acceleration deltas
- enter display sleep first
- later enter ESP32-S3 light sleep
- wake light sleep from the power button

For this repo, the first implementation should use polling through the existing
`imu_service::ReadSample(...)` API. Do not claim the IMU interrupt on `GPIO7`
in the first pass because `GPIO7` is shared with the BQ27220 interrupt path.

## Completion Rule

A milestone is complete only when every checkbox in that milestone is checked,
including the build and approval gate.

Per repo instructions, agents must not automatically run builds here. The user
will run the build and explicitly say the milestone is good to go before work
moves to the next milestone.

Use this gate for every milestone:

- [ ] All implementation requirements for the milestone are complete.
- [ ] All logging or hardware-observation requirements for the milestone are
      complete.
- [ ] The user has run a build using the existing `build/` folder.
- [ ] The user has said the result is good to go for the next milestone.

## Milestone 1: Sleep Policy Skeleton

Goal: add the service-level state machine without changing display or ESP sleep
behavior yet.

Requirements:

- [x] Add a focused `device_sleep_service` component.
- [x] Model the stages `awake`, `display_sleeping`, and `light_sleeping`.
- [x] Add settings for enabled state, display-sleep timeout, and light-sleep
      timeout.
- [x] Add app-facing inputs for user activity, motion detected, and no-motion
      started.
- [x] Add service events for entering display sleep, waking display, entering
      light sleep, and waking from light sleep.
- [x] Keep hardware actions out of the service; the service owns policy only.
- [x] Add startup logs showing configured timeout values.
- [x] Add transition logs showing previous stage, next stage, action, and reason.

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 2: IMU Inactivity Detection

Goal: classify motion/no-motion from existing direct IMU samples.

Requirements:

- [x] Add a runtime timer or task that samples `imu_service::ReadSample(...)`
      around every 200 ms.
- [x] Convert accelerometer values from `g` to `mg` before applying thresholds.
- [x] Track the previous accelerometer sample and timestamp.
- [x] Detect motion when axis-delta sum is at least `60 mg` or max-axis delta is
      at least `25 mg`.
- [x] Detect stillness only when axis-delta sum is at most `20 mg` and max-axis
      delta is at most `8 mg`.
- [x] Require a continuous 2 second stillness window before calling the
      no-motion path.
- [x] Call the motion path immediately when pickup/motion is detected.
- [x] Reset inactivity on button activity.
- [x] Reset inactivity on touch activity.
- [x] Do not enable FIFO streaming or attach the IMU interrupt on `GPIO7`.
- [x] Log motion detection with sum delta and max-axis delta.
- [x] Log no-motion arming with stillness duration, sum delta, and max-axis
      delta.

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 3: Display Sleep Demo

Goal: make the first visible staged sleep behavior work without ESP light sleep.

Requirements:

- [x] Extend `display_service` with an app-facing display-sleep command.
- [x] Render a full-screen `Display sleep` message before putting the panel to
      sleep.
- [x] Wait for the e-paper refresh to finish before calling the panel sleep
      path.
- [x] Call the existing e-paper panel sleep primitive after the message is
      visible.
- [x] Suppress normal demo selection refreshes while the display is sleeping.
- [x] Wake the display on user activity.
- [x] Wake the display on motion activity.
- [x] Restore the normal demo screen with a full refresh after wake.
- [x] Log display-sleep entry and display wake.

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 4: Light Sleep Demo

Goal: add second-stage ESP32-S3 light sleep with power-button wake.

Requirements:

- [x] Add runtime handling for the service `enter_light_sleep` event.
- [x] Render a full-screen `Light sleep` message before entering ESP light
      sleep.
- [x] Wait for the e-paper refresh to finish before entering ESP light sleep.
- [x] Put the e-paper panel into sleep before calling `esp_light_sleep_start()`.
- [x] Configure `POWER_OK` / `GPIO4` as an active-low wake source before light
      sleep.
- [x] Call `esp_light_sleep_start()` only after wake sources are armed.
- [x] On wake, disable the GPIO wake source.
- [x] Log the ESP sleep wake cause.
- [x] Treat the wake-causing power-button press as wake-only so it does not
      immediately trigger normal power-button behavior.
- [x] Wake the display with a full refresh after returning from light sleep.
- [x] Preserve normal long-press shutdown behavior while the device is awake.

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 5: Sleep Blockers And Safety

Goal: prevent auto sleep from firing during sensitive active workflows.

Requirements:

- [x] Add a provider or policy hook for app-level sleep blockers.
- [x] Block inactivity progression while recording is active, armed, saving, or
      exporting.
- [x] Block inactivity progression while a shutdown request is pending.
- [x] Block display sleep while an e-paper refresh is already in progress.
- [x] Block or defer sleep during SD-card write-heavy operations.
- [x] Decide whether USB power alone blocks sleep; default should match the
      source behavior and not block plain USB power.
- [x] Log blocker reason changes.
- [x] Make blocker logs stable and low-noise.

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 6: Configuration

Goal: make demo and production timeout values reproducible.

Requirements:

- [x] Add build-time configuration for display-sleep timeout.
- [x] Add build-time configuration for light-sleep timeout.
- [x] Use short demo defaults until hardware behavior is proven.
- [x] Document the intended production defaults separately from demo defaults.
- [x] Validate that light-sleep timeout cannot be shorter than display-sleep
      timeout unless one of the stages is disabled.
- [x] Log final resolved settings at startup.

Suggested demo defaults:

- display sleep: `10` seconds
- light sleep: `30` seconds

Suggested production defaults:

- display sleep: `180` seconds
- light sleep: `1800` seconds

Completion gate:

- [x] All implementation requirements for the milestone are complete.
- [x] All logging or hardware-observation requirements for the milestone are
      complete.
- [x] The user has run a build using the existing `build/` folder.
- [x] The user has said the result is good to go for the next milestone.

## Milestone 7: Hardware Tuning

Goal: tune behavior on the real reTerminal Sticky hardware.

Requirements:

- [ ] Confirm the device enters no-motion while flat and still on a desk.
- [ ] Confirm normal table vibration does not repeatedly wake or reset sleep.
- [ ] Confirm picking up the device wakes the display promptly from display
      sleep.
- [ ] Confirm the power button reliably wakes the device from light sleep.
- [ ] Confirm the wake-causing power-button press is consumed as wake-only.
- [ ] Confirm normal power-button long-press shutdown still works while awake.
- [ ] Measure or record observed current for awake idle, display sleep, and
      light sleep.
- [ ] Adjust motion thresholds only after recording observed false positives or
      false negatives.
- [ ] Update this document if the final thresholds differ from the initial
      source thresholds.

Completion gate:

- [ ] All implementation requirements for the milestone are complete.
- [ ] All logging or hardware-observation requirements for the milestone are
      complete.
- [ ] The user has run a build using the existing `build/` folder.
- [ ] The user has said the result is good to go for the next milestone.

## Milestone 8: Optional FIFO Or Interrupt Optimization

Goal: improve power or responsiveness only after the polling version is proven.

Requirements:

- [ ] Decide whether polling power cost is unacceptable based on measurement.
- [ ] Design shared `GPIO7` ownership between BQ27220 and IMU before attaching
      any ISR.
- [ ] Keep BQ27220 interrupt diagnostics working if the IMU interrupt path is
      added.
- [ ] Add FIFO-backed sampling only if it materially improves behavior or power.
- [ ] Add IMU interrupt handling only if shared-line behavior is understood and
      tested.
- [ ] Document the final shared-interrupt design in `docs/app-architecture.md`.

Completion gate:

- [ ] All implementation requirements for the milestone are complete.
- [ ] All logging or hardware-observation requirements for the milestone are
      complete.
- [ ] The user has run a build using the existing `build/` folder.
- [ ] The user has said the result is good to go for the next milestone.
