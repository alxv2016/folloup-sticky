# ESP32 Device Terminal / Service Console (ESP-IDF)

Use this reference when a USB/serial console path is available and not reserved by product functionality. Default behavior: proactively add a basic on-device terminal for serviceability, observability, and tuning.

## When to Add It (Default Policy)

- Add a terminal by default if:
  - a USB/serial transport is available (`USB CDC`, `USB-Serial-JTAG`, or board USB-UART)
  - it is not already dedicated to another product feature/protocol
  - project/security requirements do not forbid an interactive console
- Do not wait for the user to ask explicitly if the above conditions are met.

## Transport Selection (ESP32 Variant Aware)

- `ESP32-S2/S3`: native USB device options (often USB CDC) may be available depending board design and stack usage.
- `ESP32-C3/C6/S3`: `USB-Serial-JTAG` may be available and convenient for service console / monitor workflows.
- `ESP32` (classic): often uses external USB-UART bridge to UART console.

Before implementation, confirm:
- which transport is physically wired to the host
- whether the transport is already used for another runtime protocol
- whether JTAG/debug access must be preserved

## Preferred Implementation Approach

- Prefer ESP-IDF console primitives (`esp_console`) and REPL helpers over custom command parsers.
- Prefer built-in line editing/history/completion support (linenoise-backed REPL in ESP-IDF).
- Register commands in a small command registry rather than writing a monolithic `if/else` parser.
- Keep command handlers fast and deterministic; defer long operations to tasks if needed.
- Reuse `assets/templates/esp-console/` as the starting point for command registration.

Why:
- better UX (history, completion, help)
- consistent command parsing
- easier extension and review

## UX Requirements (User Friendly)

- Autocomplete for command names (and key subcommands where practical)
- `help` command with short descriptions
- clear error messages and usage hints
- stable command naming (`settings`, `status`, `tasks`, `heap`, `log`, `reboot`)
- consistent output format (human-readable first, optionally script-friendly)

## Minimum Useful Command Set (Recommended)

- `help`: list commands and usage
- `status`: uptime, firmware version, target, reset reason, connectivity state
- `settings get <key>` / `settings set <key> <value>`: application-space settings (with validation)
- `settings save` / `settings load` (if settings are not auto-persisted)
- `tasks`: RTOS task list / states / stack high-water marks (build-config dependent)
- `heap`: free/min/largest block (capability-specific variants if useful)
- `log level <tag|*> <level>`: runtime log tuning for noisy vs app components
- `reboot`: controlled restart (with confirmation option for production tools)

Optional high-value commands:
- `wifi status` / `wifi reconnect`
- `i2c scan` (careful: only when safe on deployed hardware)
- `display test` (format-validated patterns)
- `nvs dump` / `nvs get` (avoid exposing secrets)

## RTOS Debug Info (Expose Carefully)

- Provide lightweight snapshots, not long blocking reports.
- Common useful outputs:
  - task name / state / priority
  - stack high-water mark
  - CPU usage/runtime stats (if configured)
- Some advanced RTOS stats require `sdkconfig` options (trace/runtime stats support). Confirm and enable intentionally.
- Avoid commands that destabilize timing in production.

## Settings Interface Design

- Validate values before applying.
- Separate `set` from `save` when persistence side effects matter.
- Emit exact validation errors (`out of range`, `unsupported enum`, `requires reboot`).
- Log settings changes with source (`terminal`) and timestamp/uptime if available.
- Do not expose secrets in plain text by default.

## Logging Integration

- Terminal should complement logs, not replace them.
- Add runtime log-level commands to tune noisy components down and app modules up.
- Keep terminal output concise to avoid interfering with monitor readability.

## Security / Production Constraints

- If the product has security requirements, gate sensitive commands behind:
  - compile-time feature flags
  - build profile (dev vs prod)
  - authentication / challenge-response (if required)
- Disable or reduce destructive commands (`erase`, unrestricted memory poke) unless explicitly required.

## Review Checklist

- Console transport ownership is confirmed (USB/CDC/JTAG/UART not conflicting).
- `esp_console`/REPL used instead of ad hoc parser (unless justified).
- Autocomplete/help/history are enabled and usable.
- Command handlers validate inputs and report actionable errors.
- RTOS/heap diagnostics are bounded and safe.
- Settings commands protect secrets and persistence behavior is explicit.
- Security/build-profile gating is applied where needed.
