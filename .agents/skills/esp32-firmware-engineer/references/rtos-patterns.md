# ESP32 FreeRTOS Patterns (ESP-IDF)

Use this reference for ESP32/ESP-IDF tasking, synchronization, ISR handoff, timers, and watchdog-safe concurrency design.

## Scope and Defaults

- Target ESP-IDF projects using the built-in FreeRTOS integration.
- Prefer standard FreeRTOS APIs plus ESP-IDF integrations (`esp_event`, `esp_timer`, `esp_task_wdt`) over custom schedulers.
- Treat dual-core behavior as an explicit design concern on classic ESP32/ESP32-S3. Do not assume dual-core on all ESP32 variants (for example, ESP32-C3 is single-core).

## Task Design Patterns

### Periodic Task (No Drift)

- Use `vTaskDelayUntil()` for periodic sampling/control loops.
- Measure execution time and log misses if the loop can overrun its period.
- Keep peripheral transactions bounded with timeouts.

```c
static void sensor_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);

    for (;;) {
        read_sensor_once_with_timeout();
        vTaskDelayUntil(&last, period);
    }
}
```

### Event-Driven Task

- Prefer a queue when payload data must be transferred.
- Prefer task notifications when only signaling/bit flags are needed (lower overhead than semaphores).
- Prefer event groups for multi-subsystem readiness gates.

## ISR to Task Handoff (ESP32-Specific Constraints)

- Keep ISRs short and `IRAM`-safe if they can run while flash cache is unavailable.
- Use `IRAM_ATTR` on time-critical ISRs and ensure called functions/data are in IRAM/DRAM as required by the interrupt context.
- Use `xQueueSendFromISR()`, `xTaskNotifyFromISR()`, or `vTaskNotifyGiveFromISR()` only.
- Call `portYIELD_FROM_ISR()` when a higher-priority task was woken.
- Never call blocking ESP-IDF driver APIs from an ISR.

```c
static TaskHandle_t s_worker_task;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t hp_task_woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_worker_task, &hp_task_woken);
    if (hp_task_woken) {
        portYIELD_FROM_ISR();
    }
}
```

## Core Affinity and Priority Guidance

- Do not pin tasks to a core unless there is a clear reason (latency, driver affinity, cache behavior, isolation).
- Use `xTaskCreatePinnedToCore()` only when measured behavior requires it.
- Review priority inversions when multiple tasks share I2C/SPI/UART/network resources.
- Avoid long blocking calls in high-priority tasks; they commonly trigger watchdog symptoms.

## Synchronization Patterns

### Mutexes

- Use mutexes for shared peripheral buses (I2C/SPI) or shared state with non-trivial critical sections.
- Keep lock hold time short; do not log heavily while holding a mutex.
- Prefer one owner task for complex peripherals instead of many tasks sharing a mutex.

### Critical Sections

- Use only for very short, bounded read-modify-write operations.
- Avoid wrapping driver calls, logging, or queue operations in critical sections.
- Remember critical sections can increase interrupt latency and watchdog risk.

## Timers: Pick the Right Tool

- `esp_timer`: high-resolution callbacks, deferred work scheduling, microsecond time base.
- FreeRTOS software timers: lightweight task-context timer callbacks, millisecond-scale periodic work.
- `gptimer` driver: hardware timer peripheral, waveform/capture/tighter timing use cases.

Rule of thumb:
- Control loop / task cadence -> task + `vTaskDelayUntil()`
- App-level retry/backoff -> FreeRTOS timer or `esp_timer`
- Precise peripheral timing/capture -> hardware timer (`gptimer`, RMT, LEDC depending use case)

## ESP-IDF Event Loop Integration

- Use `esp_event` for Wi-Fi, IP, and other subsystem events instead of ad hoc polling.
- Keep event handlers small; defer heavy work to a task/queue.
- Track handler registration/unregistration to avoid duplicate callbacks and leaks.

## Memory and Stack Monitoring (RTOS-Focused)

- Monitor stack margins with `uxTaskGetStackHighWaterMark()` during testing.
- Watch heap health with `heap_caps_get_free_size()` and `heap_caps_get_minimum_free_size()`.
- Treat task stack size as a design parameter, especially for logging, JSON/TLS, and protocol parsing paths.

## Watchdog and Liveness

- Use `esp_task_wdt` for long-running tasks in production where appropriate.
- Feed/monitor watchdogs intentionally; do not “fix” watchdog resets by disabling the watchdog first.
- Investigate root causes: deadlocks, long critical sections, busy loops, blocked callbacks, or starved lower-priority tasks.

## Runtime Introspection via Service Terminal (Recommended)

- If a USB/serial service terminal exists, expose bounded RTOS diagnostics commands:
  - task list/state/priority
  - stack high-water marks
  - heap/min-free snapshots
- Prefer snapshot-style commands over long-running reports.
- Ensure diagnostic commands do not block critical tasks or hold shared locks for long.

## Review Checklist (Merged and ESP32-Adapted)

- Use `vTaskDelayUntil()` for periodic tasks to avoid drift.
- Keep ISRs short and defer work to tasks via queue/notification/event bits.
- Verify ISR-safe API usage and IRAM safety for ISR paths.
- Use task notifications when payload transfer is not needed.
- Size stacks from measurement, not guesswork.
- Prefer mutexes over long critical sections; check priority inversion exposure.
- Monitor heap/stack during bring-up and regression testing.
- Confirm watchdog strategy for production builds.
- If a service terminal exists, verify RTOS diagnostics commands are safe and bounded.
