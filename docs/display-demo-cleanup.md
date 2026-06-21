# Display Demo Cleanup

The current display stack now has a real startup splash, a reusable status bar,
and sleep/shutdown indicators wired into runtime state. What remains is the
temporary demo bridge used for the main content surface.

This document tracks the cleanup needed to move from that bridge to real views
ported from `followup`.

## Current Bridge State

The remaining demo-specific path is concentrated in:

- `components/display_service/display_service.cpp`
- `components/display_service/include/display_service.h`
- `main/app_shell.cpp`

Current demo artifacts:

- `DemoSelection`
- `SelectDemoSelection(...)`
- `RequestDemoSelection(...)`
- `DrawDemoFrame(...)`
- `"FORMAT"` / `"SD"` demo card content
- `"format_sd"` naming in app-shell routing and logging

Sleep and shutdown are no longer using separate demo text screens. Those flows
now render the status-bar indicators immediately before:

- display sleep
- light sleep
- deep-sleep shutdown

## Cleanup Goals

The display API should evolve from a demo-selection model into a real
view/screen model while preserving the current architecture boundaries:

- `epaper_ui` owns reusable renderers and widgets
- `main/` runtime helpers compose product state into UI contracts
- `display_service` owns framebuffer composition, refresh policy, and panel
  sleep/wake behavior
- `app_shell` stays orchestration-only

## TODO

1. Replace `DrawDemoFrame(...)` with the first real view renderer ported from
   `followup`.
2. Rename `DemoSelection` to a view-oriented type such as `ViewId`, `ScreenId`,
   or `PageId`.
3. Rename `SelectDemoSelection(...)` and `RequestDemoSelection(...)` so the API
   reflects view navigation rather than a temporary demo state machine.
4. Remove `"format_sd"` and the `"FORMAT"` / `"SD"` demo-card content once the
   real first screen exists.
5. Move any reusable visual pieces discovered during that port into
   `components/epaper_ui` instead of re-embedding them in `display_service`.
6. Keep status-bar composition in `main/status_bar_runtime.cpp` and do not move
   service reads into `epaper_ui`.
7. Preserve the current sleep/wake sequencing: the active screen should redraw
   after wake, and sleep/shutdown indicators should remain status-bar driven.

## Migration Notes

This should be an incremental cleanup, not a big-bang rewrite.

Recommended order:

1. Port the first real view.
2. Teach `display_service` to render that real view plus the status bar.
3. Rename the public display API from demo terminology to view terminology.
4. Remove the temporary demo bridge only after the replacement view path is in
   use.
