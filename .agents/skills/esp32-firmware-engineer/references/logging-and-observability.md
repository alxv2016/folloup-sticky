# ESP32 Logging and Observability (ESP-IDF)

Use this reference when designing logs, filtering noise, or diagnosing issues from `idf.py monitor`.

## Logging Policy

- Keep application-space logs verbose and high signal during development/debugging.
- Reduce irrelevant library/default component logs when they hide the application's state transitions.
- Prefer targeted filtering/tuning over global suppression.

## Practical ESP-IDF Logging Guidance

- Use stable module tags (`wifi_mgr`, `sensor_task`, `display_drv`, `ota_updater`).
- Log:
  - state transitions
  - error codes (`esp_err_t`)
  - retry counts/backoff
  - timing/latency (when relevant)
  - key configuration decisions at startup
- Avoid repeated unstructured info logs in tight loops.
- If an on-device terminal is present, expose runtime log-level controls (by tag / wildcard) so signal can be tuned without reflashing.

## Noise Reduction Strategy

- Lower noisy component log verbosity selectively (build-time config or runtime log-level control where used).
- Keep app modules at `DEBUG`/`VERBOSE` while reducing third-party/default chatter if needed.
- Preserve enough system logs to diagnose reset/panic/network events.

## What Good Logs Look Like

- Event-first and stateful:
  - `wifi_mgr: disconnected reason=... retry=3 backoff_ms=2000`
  - `display_drv: flush region x=0 y=0 w=240 h=40 fmt=rgb565`
- Include identifiers for peripherals/devices/buses when multiple instances exist.
- Include durations for timeouts and retries.

## Review Checklist

- Application logs are verbose enough to debug behavior.
- Library/default noise is reduced when it obscures signal.
- Terminal log-level commands (if present) are scoped and safe.
- Error logs include code + context, not only generic failure text.
- Startup logs capture key target/config assumptions.
- Logs do not create excessive timing disruption in hot paths.
