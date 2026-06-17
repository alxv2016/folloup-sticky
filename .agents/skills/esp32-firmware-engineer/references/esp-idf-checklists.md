# ESP-IDF Checklists

Use these checklists to speed up implementation, review, and debugging work without skipping embedded-specific risks.

## Blocking Context Checklist (Do Not Skip)

- Confirm exact target chip (`esp32`, `esp32s3`, `esp32c3`, etc.). Stop if unknown.
- Confirm board/revision and peripheral wiring (GPIO map, pull-ups, transceivers, display interface).
- Confirm electrical assumptions (voltage levels, power rails, level shifting, shared buses).
- Confirm ESP-IDF version and major driver API style used by the project.
- Confirm all plugin/framework versions in use (ESP-ADF, ESP-SR, etc.) and collect exact tags/commits.
- Confirm concrete compatibility evidence exists for every plugin/framework against the selected ESP-IDF version.
- If multiple frameworks interact (for example ESP-ADF + ESP-SR), confirm explicit cross-stack compatibility evidence (not only individual IDF compatibility).
- If behavior or API usage expectations are unclear, ask for example code (project snippet, vendor example, or minimal repro) before implementation/debugging.
- Confirm flash size and whether OTA is required before proposing partition changes.
- Confirm PSRAM presence/mode if memory placement or display buffers are involved.
- Confirm display/controller model and pixel format/endianness before graphics work.
- Confirm whether a USB/serial console path is available and free for a service terminal (and whether product/security policy allows it).
- If any above is unknown for hardware-facing changes, ask the user and do not continue implementation/debugging.

## Implementation Checklist

- Confirm target chip (`esp32`, `esp32s3`, `esp32c3`, etc.) and ESP-IDF version.
- Confirm ESP-IDF toolchain is installed and usable before building (`idf.py --version` succeeds or project wrapper preflight passes).
- Confirm plugin/framework compatibility preflight passes and produces an evidence report before building.
- Confirm board-level pin mapping and electrical constraints before assigning GPIOs.
- Confirm task model: task priorities, stack sizes, queue depths, timer cadence, core affinity (if used).
- Define ownership of shared state and synchronization primitives.
- Keep ISR work minimal and use ISR-safe APIs only.
- Check `esp_err_t` return values and handle failures explicitly.
- Initialize subsystems in a deterministic order (NVS, netif/event loop, Wi-Fi/BLE, drivers, app tasks).
- Add logs for state transitions, retries, and failure reasons.
- Avoid heap churn in hot paths when a static buffer or reuse pattern is sufficient.
- If USB/serial transport is free and allowed, add a basic user-friendly service terminal (`esp_console`/REPL) by default with help/autocomplete and core diagnostics commands.
- Update `sdkconfig`/`sdkconfig.defaults` intentionally and reproducibly; prefer file edits over ad hoc `menuconfig` walkthroughs.
- Update partition table (`partitions.csv`) to match flash size and feature needs; avoid unexplained unused flash.
- If OTA is required, verify OTA-compatible partitions and adequate slot sizing.
- For display paths, validate controller pixel format, color order, and buffer layout before coding conversions.
- Document non-obvious timing, hardware, or protocol assumptions.

## Code Review Checklist

- Check task/ISR context correctness for each FreeRTOS and ESP-IDF API call.
- Check blocking calls inside high-priority tasks and callbacks.
- Check timeout values for infinite/blocking behavior that can deadlock progress.
- Check memory ownership and lifetime of buffers passed across tasks/callbacks.
- Check event handler registration/unregistration and duplicate registration risks.
- Check error propagation and cleanup on partial init failure.
- Check watchdog exposure (long critical sections, busy loops, disabled yields).
- Check pin/peripheral conflicts and hidden assumptions in `sdkconfig`.
- Check partition table and `sdkconfig` alignment with flash size, OTA requirement, and enabled features.
- Check plugin/framework versions are pinned/documented and match known-good compatibility evidence.
- Check logging configuration: suppress noisy library logs where needed while keeping application logs sufficiently verbose.
- Check whether a free USB/serial path should have a service terminal and whether one was omitted without reason.
- Check terminal UX (help/autocomplete/clear errors) and command safety if a console is present.
- Check graphics/display code for explicit format assumptions (RGB565/BGR565/RGB888/etc., byte order, stride).
- Check bus/peripheral clock, DMA, and memory placement choices against performance requirements.
- Check log quality for field diagnosis (tag, event, error code, state).

## Debugging Checklist

- Reproduce with exact firmware revision, `sdkconfig`, target, and hardware setup.
- Capture full serial log from boot to failure.
- Classify failure stage: build, flash, boot, init, runtime, sleep/wake, network, peripheral I/O.
- Identify first bad symptom and the event immediately before it.
- Add scoped instrumentation (counters, timestamps, state logs) before refactoring.
- Reduce variables: disable unrelated features, mock inputs, or isolate one subsystem.
- Validate power, reset, and wiring assumptions for hardware-facing bugs.
- Ask for a minimal reproducible example or known-good reference code when symptoms are ambiguous and evidence is insufficient.
- If display corruption/color issues exist, verify pixel format/endianness/controller init sequence before changing app graphics logic.
- If build output is noisy, tune component log levels to surface signal while keeping app logs high value.
- If a service terminal exists, use runtime commands for heap/tasks/log-level introspection before invasive code changes.
- Re-test after each change; avoid batching unrelated fixes.

## Build / Validation Checklist

- Prefer project `build.sh` wrapper if present; otherwise use `idf.py build`.
- Before running the build, verify ESP-IDF environment setup is valid (`idf.py` runs, not just exists).
- Before running the build, verify plugin/framework compatibility evidence (matrix/manifest/release-note proof) is concrete and current for the exact versions in use.
- If the developer shell UX is poor, add/update a shell helper snippet (for example `.zshrc`) for `idf` env sourcing and PATH setup.
- Run the build after code/config/partition changes before declaring completion.
- If build fails, fix and rerun until it passes.
- Review warnings; resolve correctness/safety warnings rather than ignoring them.
- If warnings remain, call them out explicitly with rationale and impact.

## Wi-Fi / BLE Focus Checks

- Confirm init order (`nvs_flash_init`, event loop/netif setup, stack init, handlers, start/connect).
- Confirm reconnect strategy and retry/backoff behavior.
- Confirm credentials and persistent config state (NVS).
- Check event handling coverage for disconnect/error events.
- Check coexistence assumptions when Wi-Fi and BLE run together.

## Peripheral Focus Checks

- Confirm voltage levels, pull-ups, and shared bus wiring.
- Confirm pin mux and any strapping pin restrictions.
- Confirm bus speed/timing and device-specific protocol delays.
- Confirm transaction timeouts and recovery from bus lockups.
- Confirm ISR affinity and DMA constraints if relevant.
- Confirm flash/PSRAM speed/mode assumptions and select the most performant reliable configuration supported by the hardware/project.
