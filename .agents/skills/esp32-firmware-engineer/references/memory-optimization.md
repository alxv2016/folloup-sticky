# ESP32 Memory and Size Optimization (ESP-IDF)

Use this reference for RAM/flash/code-size optimization and memory-safety decisions in ESP-IDF projects.

## ESP32 Memory Model (Practical View)

- Internal RAM is limited and shared with stacks, drivers, and protocol stacks.
- Some features (Wi-Fi/BLE, networking, TLS) increase internal RAM pressure significantly.
- PSRAM may be available on some modules/targets, but not all memory is equal:
  - Latency differs from internal RAM
  - DMA compatibility is constrained
  - Some ISR/critical paths should stay in internal memory
- Use ESP-IDF heap capabilities APIs when memory class matters (`heap_caps_*`).
- Performance depends on memory placement and bus mode, not only free bytes; internal RAM, PSRAM, and flash-backed code/data have different latency/throughput behavior.

## Allocation Policy (What to Prefer)

- Prefer static allocation for long-lived buffers and core control structures.
- Reuse work buffers in non-overlapping paths.
- Avoid heap allocation in hot paths and callback-heavy paths.
- Never allocate from ISR context.
- Use fixed-size pools when bounded dynamic behavior is needed.

## DMA / Capability-Aware Allocation

- Allocate DMA buffers with capability flags (for example `MALLOC_CAP_DMA`) when required by SPI/I2S/peripheral drivers.
- Verify buffer lifetime across async transactions (queued SPI/UART/etc.).
- Do not assume PSRAM buffers are valid for every DMA path.

## Stack Sizing and Monitoring

- Start with conservative task stacks for parsing/logging/network code, then measure and tighten.
- Monitor stack high-water marks (`uxTaskGetStackHighWaterMark()` / `uxTaskGetStackHighWaterMark2()` depending config/API availability).
- Watch for hidden stack growth from:
  - Large local arrays
  - Deep call chains
  - Logging and format strings
  - JSON/TLS/protocol parsers

## Heap Monitoring and Fragmentation Awareness

- Track:
  - `heap_caps_get_free_size(...)`
  - `heap_caps_get_minimum_free_size(...)`
  - Largest free block if fragmentation is suspected
- Measure before/after feature init and under steady-state runtime.
- Repeated alloc/free of variable-sized buffers is a common fragmentation source.

## Code Size Optimization (ESP-IDF Workflow)

- Use `idf.py size` and `idf.py size-components` to identify growth.
- Inspect the linker map (`build/<app>.map`) when component-level output is not enough.
- Prefer `const` data for read-only tables and strings.
- Keep component dependencies minimal; unused components can pull in surprising code.
- Review logging levels and format-heavy debug code in release builds.
- If code execution speed matters, check whether hot code/data placement and flash/PSRAM configuration (`sdkconfig`) are limiting throughput.

Common levers (project-dependent):
- `sdkconfig` optimization level (`CONFIG_COMPILER_OPTIMIZATION_*`)
- Link-time optimization (if enabled/supported in project/toolchain setup)
- Reducing enabled features/components/protocols

## Data Structure and Buffer Patterns

- Use the smallest type that matches protocol range and alignment requirements.
- Pack structures only when layout compatibility is required (protocol/on-flash format); avoid unnecessary packed structs in hot code due to alignment penalties.
- Prefer ring buffers/stream buffers for byte streams.
- Use explicit ownership comments for buffers that cross task boundaries.

## Flash / NVS / Partition Considerations

- Use NVS for small persistent config/state instead of ad hoc raw flash writes.
- For write-heavy application data, design a wear-aware strategy (NVS, filesystem, or custom log structure with rotation).
- Keep partition table and OTA slot size in mind when code size grows.
- Validate that large assets/tables belong in firmware at all; they may fit better in filesystem or external storage.

## Compile-Time Guards

- Use `_Static_assert` / `static_assert` for:
  - protocol struct sizes
  - array lengths
  - queue payload sizes
  - compile-time configuration assumptions

## Review Checklist (Merged and ESP32-Adapted)

- Static/reused buffers preferred over ad hoc heap allocations.
- No heap allocation in ISR or time-critical paths.
- DMA buffers use capability-aware allocation when required.
- Task stacks sized from measurements; high-water marks checked.
- Heap minimum free and fragmentation indicators monitored in tests.
- `const` used for read-only data.
- Code-size growth checked with `idf.py size` / component breakdown.
- Partition/OTA/NVS implications considered for flash usage changes.
- RAM/flash/PSRAM configuration and placement choices reviewed for performance-critical paths.
