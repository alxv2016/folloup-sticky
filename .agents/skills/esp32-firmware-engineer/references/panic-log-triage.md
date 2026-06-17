# Panic and Log Triage

Use this file when diagnosing resets, panics, boot loops, and unclear runtime failures from serial logs.

## Collect the Right Data First

- Capture the full serial log from reset through failure (not only the panic tail).
- Record the exact ESP-IDF version, target chip, and build type.
- Record the command used (`idf.py monitor`, baud, serial port).
- Save the ELF file that matches the flashed binary for symbol resolution.

## Reset / Failure Categories

- Build/link failure: compiler, linker, component dependency, or config mismatch.
- Flash/connection failure: serial port, permissions, cable, boot mode, stub/baud issues.
- Boot failure: partition table, image mismatch, early init crash, missing config/data.
- Runtime panic: null dereference, stack overflow, illegal instruction, watchdog timeout.
- Runtime functional failure: no panic, but incorrect behavior, timeouts, or lost connectivity.

## Panic Triage Flow

1. Identify reset reason / panic headline.
2. Read the lines immediately before the panic for the triggering subsystem.
3. Decode backtrace against the matching ELF (use monitor decoding or addr2line workflow).
4. Inspect the top frames and the first app frame.
5. Check recent changes touching that subsystem, task, buffer, or callback path.
6. Add focused logs/asserts around the suspected boundary.

## Common Embedded Root Causes to Check

- Null/uninitialized handles after partial init failure.
- Stack overflow in task with logging, JSON parsing, TLS, or BLE/Wi-Fi callbacks.
- Use-after-free or buffer lifetime crossing task boundaries.
- Calling non-ISR-safe APIs from an ISR or callback context.
- Race conditions around shared flags/queues without synchronization.
- Watchdog due to blocking loop, deadlock, or long critical section.
- Misconfigured pins/peripherals causing driver timeouts that cascade into watchdog resets.

## Logging Guidance

- Use stable log tags per module (`wifi_mgr`, `sensor_task`, `ble_gatt`, etc.).
- Log state transitions and error codes, not only generic failure text.
- Include retry counts and timeout durations when diagnosing reconnect loops.
- Add temporary high-signal logs, then remove or downgrade once fixed.

## Useful Commands (Adapt to Project)

- `idf.py build`
- `idf.py flash monitor`
- `idf.py fullclean build`
- `idf.py menuconfig`

Note version-specific output and panic formatting may differ across ESP-IDF releases. Prefer interpreting logs with the project's actual ESP-IDF version and matching ELF artifacts.
