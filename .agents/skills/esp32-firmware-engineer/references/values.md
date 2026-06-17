# ESP32 Firmware Engineering Values

Use this file first. It defines non-negotiable behavior for the ESP32 firmware skill.

## 1. Hardware Truth Before Code

- `Value`: Firmware must match the actual hardware, not an assumed board.
- `Why`: Most embedded failures come from incorrect pin maps, electrical assumptions, or missing peripheral context.
- `Do`:
  - Confirm exact ESP32 variant.
  - Confirm peripheral inventory, wiring, buses, power rails, transceivers, and display/controller details.
  - Ask for missing information before implementation or debugging.
- `Avoid`:
  - Guessing pins, interfaces, pull-ups, voltage levels, or controller models.
  - Porting generic examples directly into ESP-IDF projects.
- `Blocking rule`: If hardware context is incomplete for hardware-facing tasks, stop and ask.

## 2. Variant Certainty Is Mandatory

- `Value`: Never proceed with an unknown ESP32 target.
- `Why`: ESP32 variants differ materially (cores, peripherals, memory, radio/peripheral capabilities, sleep/wakeup features).
- `Do`: Require exact target (`esp32`, `esp32s3`, `esp32c3`, etc.) and ESP-IDF version.
- `Avoid`: Writing code that assumes dual-core behavior, peripheral availability, or wakeup features across variants.
- `Review cues`:
  - Does the code assume a peripheral not present on the target?
  - Does it assume dual-core/pinning where the chip is single-core?

## 3. Configuration and Partitions Are Source Code

- `Value`: `sdkconfig` and partition tables are part of the deliverable, not afterthoughts.
- `Why`: Many runtime failures and performance issues are configuration-driven, not code-driven.
- `Do`:
  - Edit `sdkconfig` / `sdkconfig.defaults` deliberately and reproducibly.
  - Size partitions to fit the actual flash and feature set.
  - Use OTA-compatible layouts when OTA is required.
  - Avoid leaving unexplained flash capacity unused.
- `Avoid`:
  - “Use menuconfig and click around” as the primary workflow.
  - Partition guesses without flash-size/OTA requirements.

## 4. Build-Proven Changes Only

- `Value`: A change is not done until the project builds cleanly in the project workflow.
- `Why`: Embedded breakage often appears in generated config, link stage, or warnings that indicate real bugs.
- `Do`:
  - Verify ESP-IDF tooling is installed and usable before the build step (not just "probably installed").
  - Run project `build.sh` after changes (or equivalent build wrapper).
  - Fix failures and rerun until it passes.
  - Treat important warnings as work to resolve, not noise.
- `Avoid`: Declaring completion based only on reasoning or partial compilation.

## 5. Compatibility Evidence Before Progress

- `Value`: Version compatibility is a proof obligation, not a guess.
- `Why`: ESP-IDF + ESP-ADF + ESP-SR stacks can fail unless versions match exact supported combinations.
- `Do`:
  - Identify exact versions/tags/commits for every plugin/framework in use.
  - Collect concrete evidence from official matrices/manifests/release notes.
  - Require explicit cross-stack evidence when multiple frameworks interact (for example ADF + SR).
- `Avoid`:
  - Assuming “latest with latest” is compatible.
  - Proceeding on anecdotal compatibility without version proof.
- `Blocking rule`: If any plugin compatibility link is unproven, stop before build/debug/flash.

## 6. High-Signal Observability

- `Value`: Suppress noise, increase application signal.
- `Why`: Embedded debugging depends on logs, but noisy defaults hide causality.
- `Do`:
  - Reduce irrelevant library/default component logs when diagnosing.
  - Keep application logs verbose, tagged, and stateful.
  - Log error codes, retries, timings, and transitions.
- `Avoid`:
  - Global log suppression that removes evidence.
  - Generic “failed” logs without context.

## 7. Serviceability by Default (When Console Transport Is Free)

- `Value`: If a USB/serial console path is available and unused, ship a basic terminal by default.
- `Why`: Runtime inspection and settings control dramatically reduce debug iteration time and field diagnosis effort.
- `Do`:
  - Use ESP-IDF console/REPL with help, history, and autocomplete.
  - Expose safe commands for settings, status, heap, RTOS diagnostics, and log levels.
  - Keep command handlers bounded and user-friendly.
- `Avoid`:
  - Ad hoc parsers with poor error messages.
  - Debug-only commands that destabilize timing or expose secrets.
- `Blocking rule`: Confirm transport ownership and security policy before enabling the terminal.

## 8. Correct Data Format First (Especially Displays)

- `Value`: Data format correctness comes before graphics logic or performance tuning.
- `Why`: Display bugs are often pixel format, byte order, stride, or controller-init mismatches.
- `Do`:
  - Confirm controller, bus mode, resolution, pixel format, endian/color order, and flush region format.
  - Convert only to the exact format the display path expects.
- `Avoid`:
  - Guessing RGB/BGR order or assuming RGB565 packing.
  - “Fixing” color problems by random bit swapping.

## 9. Performance Choices Must Respect Hardware Limits

- `Value`: Use the most performant reliable option supported by the actual hardware.
- `Why`: Throughput depends on bus speed, DMA capability, memory placement, flash/PSRAM modes, and signal integrity.
- `Do`:
  - Check RAM/flash/PSRAM capabilities and bus timing limits.
  - Use DMA-capable memory where required.
  - Measure, then tune.
- `Avoid`:
  - Benchmarking assumptions detached from board wiring and clock config.
  - Chasing speed with unstable settings.

## 10. Explicit Unknowns, Explicit Risks

- `Value`: State what is known, unknown, and unverified in hardware.
- `Why`: Embedded software can appear correct in code review while failing on real boards.
- `Do`: Call out missing hardware validation and remaining assumptions in the final response.
- `Do`: Ask for example code (project snippet, known-good implementation, or minimal repro) when uncertainty would otherwise force guessing.
- `Avoid`: Implying hardware verification when only build/log review was performed.
- `Avoid`: Filling gaps with guessed API usage or assumed behavior when an example can be requested.
