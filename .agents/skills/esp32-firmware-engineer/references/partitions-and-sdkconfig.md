# ESP32 Partitions and sdkconfig (ESP-IDF)

Use this reference when changing flash layout, OTA support, or project configuration.

## Core Rules

- Treat `sdkconfig` and partition CSV as first-class project artifacts.
- Prefer editing config files (`sdkconfig`, `sdkconfig.defaults`, Kconfig fragments where used) over interactive `menuconfig` instructions for reproducibility.
- Do not propose partition changes until flash size and OTA requirement are confirmed.
- Use the available flash capacity intentionally; avoid unexplained empty regions.

## What to Confirm Before Partition Changes

- Exact flash size (for example 4MB, 8MB, 16MB)
- OTA requirement (single app vs dual-slot OTA, rollback needs)
- NVS size needs (Wi-Fi creds, app config, calibration data)
- Filesystem/data partition needs (SPIFFS/LittleFS/FATFS if used)
- Core dump partition requirement (if enabled)
- Factory app partition requirement (some products need it; many do not)

## Partition Strategy Guidelines

### No OTA Required

- Prefer a larger app partition plus appropriately sized NVS/data partitions.
- Avoid reserving OTA slots unless they are actually needed.

### OTA Required

- Use OTA-compatible layout (typically `otadata` + two OTA app slots).
- Size OTA slots based on current binary size plus growth headroom.
- Ensure partition choices align with `sdkconfig` OTA and bootloader settings.
- If rollback is used, ensure configuration and partitioning support it.

## Flash Utilization Policy

- Every partition should have a reason.
- Free space should either:
  - be assigned as growth headroom with an explicit note, or
  - be allocated to useful data/app capacity.
- Do not leave large gaps because of copied example layouts that do not match the target flash.

## sdkconfig Editing Workflow (Reproducible)

- Read current `sdkconfig` and the relevant component/project defaults.
- Change only the required keys.
- Keep related settings in sync (example: target, flash size, log levels, PSRAM, partition table options).
- Explain why each configuration change was made.
- Prefer checking the resulting `sdkconfig` diff over hand-wavy menu navigation steps.

## menuconfig Policy

- `menuconfig` is a discovery/debug tool, not the primary delivery artifact.
- If `menuconfig` is used to discover an option, reflect the final change in `sdkconfig`/defaults and show the exact config keys.
- Do not leave the user with only "open menuconfig and click X" guidance unless explicitly requested.

## Partition / Config Review Checklist

- Exact flash size confirmed.
- OTA requirement confirmed.
- Partition table matches feature set and flash capacity.
- App slot sizes include realistic headroom.
- NVS/data/core dump partitions sized intentionally.
- `sdkconfig` partition-table selection points to the correct CSV.
- Log level, PSRAM, and flash/boot settings are consistent with performance/debug goals.
- No copied example layout remains without justification.
