# ESP32 Communication Protocols (ESP-IDF)

Use this reference for ESP32 peripheral communication patterns in ESP-IDF: I2C, SPI, UART, and TWAI (CAN).

## Scope and Version Notes

- ESP-IDF has both legacy and newer driver APIs in some subsystems (notably I2C/ADC across versions).
- Prefer the project's existing API style unless you are explicitly migrating.
- Always confirm target chip and pin map before coding (ESP32 vs ESP32-S3/C3/C6 feature differences, pin capabilities, strapping pins).

## I2C (Master) Patterns

- Confirm external pull-ups, bus voltage compatibility, and bus speed before debugging software.
- Always use transaction timeouts; never wait forever on a busy bus.
- Handle bus recovery/reset on repeated timeout conditions (device lockups are common).
- Serialize access with a mutex or a dedicated bus-owner task.
- Log address, register, timeout, and error code for field diagnostics.

Common ESP-IDF patterns:
- Legacy API: `i2c_param_config()`, `i2c_driver_install()`, command links.
- Newer API (IDF v5+): bus/device handles with explicit device configuration and transfer timeout.

## SPI (Master) Patterns

- Use `spi_bus_initialize()` and `spi_bus_add_device()` and keep device config (mode, clock, CS, queue depth) explicit.
- For DMA transfers, allocate buffers with DMA-capable memory (`MALLOC_CAP_DMA`) when required.
- Be explicit about transaction ownership if multiple tasks share the bus.
- Validate max clock against wiring length, signal integrity, and device timing, not just the data sheet headline.
- Prefer queued transactions for throughput; prefer synchronous transmit for simple control paths.

Review cues:
- Are TX/RX buffers valid for the duration of the transaction?
- Is CS behavior correct for multi-part register operations?
- Are DMA-capable buffers used where needed?

## UART Patterns

- Prefer ESP-IDF UART driver (`uart_driver_install`) with the driver ring buffer/event queue before writing a custom ISR buffer.
- Use a dedicated parser task for framed protocols.
- Bound parser work and handle malformed frames/noise.
- If using an ISR callback path, keep it minimal and IRAM-safe as needed.

Typical architecture:
- UART driver ISR/ring buffer -> parser task -> application queue/state machine

## TWAI (CAN) Patterns

- On ESP32-class chips with TWAI support, use the TWAI driver (`twai_driver_install`, start/stop/transmit/receive, alerts).
- Validate transceiver wiring and termination first; software often gets blamed for bus electrical issues.
- Use alerts and error counters to distinguish bus-off/warning states from application bugs.
- Implement recovery logic for bus-off instead of retrying transmit forever.

## RMT / Special Protocol Note

- For timing-sensitive one-wire / IR / pulse protocols, prefer RMT over bit-banging in tasks.
- RMT often reduces jitter and CPU load compared with software timing loops.

## Shared Communication Bus Design

- Prefer a bus manager task when:
  - Multiple tasks issue transactions
  - Ordering matters (sensor init + reads + calibration writes)
  - Retries/recovery must be centralized
- Use a mutex only when transactions are short and ownership is simple.

## Error Handling and Recovery (Merged and ESP32-Adapted)

- Always use timeouts to prevent deadlocks/stalls.
- Propagate and log `esp_err_t` (plus protocol-level status if available).
- Implement retry with backoff for transient faults; avoid tight retry loops.
- Distinguish hardware faults (wiring/pull-up/power) from protocol framing/software faults.
- Validate received payloads (CRC/checksum/length/state machine transitions).

## Hardware and Pin Constraints (ESP32-Specific)

- Check GPIO matrix routing limits and peripheral pin capability for the selected chip.
- Watch strapping pins and boot mode interactions.
- Confirm voltage levels (3.3V logic, open-drain pull-ups for I2C, transceiver requirements for TWAI/RS-485).
- Confirm bus speed/timing against cable length and pull-up strength.

## Review Checklist

- Timeouts present on all protocol operations.
- Shared bus access serialized correctly (mutex or owner task).
- ISR-safe APIs used in ISR paths only.
- DMA-capable buffers used when needed.
- Protocol parsing validates length/state/CRC.
- Logs include enough context (bus, device addr/id, op, error, timeout).
- Recovery path exists for bus lockups / device reset / bus-off.
