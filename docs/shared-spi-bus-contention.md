# Shared SPI Bus Contention (Display vs. SD)

## Summary

The SSD1677 e-paper panel and the MicroSD card share one SPI bus (`SPI2_HOST`):
they share `SCK` (`GPIO13`), `MOSI` (`GPIO14`), and `MISO` (`GPIO12`), with
separate chip selects (`EP_CS` on `GPIO15`, `SD_CS` on `GPIO8`). Because it is a
single physical bus, only one device may transact at a time.

Every access to that bus **must** be serialized through
`components/shared_bus_service`. If SD I/O runs while the display is mid-refresh,
the SD transactions splice into the panel's command/waveform stream and corrupt
the refresh, leaving **e-paper ghosting**. The fix is mutual exclusion, not a
heavier refresh.

## Symptoms

- Ghosting on the dashboard right after saving/tagging a recording — the area a
  modal (e.g. the recording tag select modal) had occupied did not clear.
- The ghosting correlated with a partial refresh coinciding with the SD write.
- `DisplayService: refresh metrics` showed the SPI transfer time spike during a
  save, e.g. `spi=126136us` vs the normal `spi≈40000us` — the display transfer
  was being throttled by concurrent SD traffic on the bus.
- A red herring: tagging a recording as **Note** looked fine while **Task**/
  **Idea** ghosted. That was not tag-specific behavior — Note is the default-
  focused tag (no focus move), so its refresh happened to win the bus before the
  SD write started, while Task/Idea's refresh collided with the write.

## Root cause

`storage_service::RunWithMountedFilesystem` — the path used by every recording
save, transcript write, and archive rescan — held only the card mutex and did
**not** take the shared-bus `StorageBusGuard`, unlike the SD format path and
every display refresh. So SD I/O ran concurrently with e-paper refreshes on the
same host. On the display side each refresh holds the bus via `DisplayBusGuard`
(`shared_bus_service::AcquireDisplay`), but with storage not participating in the
same mutex there was nothing to stop SD transactions from interleaving — and the
e-paper panel uses manual CS with multi-transaction command/data sequences, so an
interleaved SD transaction corrupts a refresh in progress.

## Fix

1. **Serialize SD I/O with the display.** `RunWithMountedFilesystem` now wraps
   its mount + handler in a `StorageBusGuard`, acquired **before** the card mutex
   to match the format path's lock order. SD and display now take clean turns on
   the bus; a refresh always finishes before an SD write starts (and vice versa).
   See `components/storage_service/storage_service.cpp`.

2. **Never drop a queued full refresh.** The display command queue is a single
   slot (`xQueuePeek` + `xQueueOverwrite`) that only coalesces same-type
   commands. While the display task is blocked on the bus during a save, a queued
   full refresh (screen transition, wake, de-ghost reseed) could be overwritten
   by a later partial of a different command type. `EnqueueDisplayCommand` now
   promotes the survivor to a full-screen refresh so the full is never lost. See
   `components/display_service/display_service.cpp`.

## What we deliberately did NOT do

We briefly forced a **full** e-paper refresh on every overlay/modal dismiss
(`OverlayRefreshPolicy::kRebuildUnderlayFull`) to "clear the ghost." That was
treating the symptom: it added an expensive (~1.65s) full-screen flash to every
modal close and did not address the real cause. Once SD I/O is serialized with
the display, a **partial** refresh finishes uncorrupted and clears cleanly, so
overlay dismissals use `kRebuildUnderlay` (partial). Do not reintroduce a
forced full refresh on dismiss to fix ghosting — fix bus serialization instead.

## Rule for future work

Any new access to a device on the shared `SPI2_HOST` bus (SD, e-paper, or a new
peripheral added to it) **must acquire the bus through `shared_bus_service`**
(`AcquireStorage` / `AcquireDisplay`, via the `StorageBusGuard` / `DisplayBusGuard`
RAII helpers) for the full duration of the transaction before touching the bus.
Never issue SPI transactions to a shared-bus device while holding only a
device-level lock. Related bring-up constraint: with an SD card inserted,
initialize MicroSD first and keep it mounted before bringing up the e-paper
panel (see Hardware Notes in `app-architecture.md`).
