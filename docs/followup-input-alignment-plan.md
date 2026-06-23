# Followup Sticky Focus Refactor Plan

This document tracks the refactor work needed to make `folloup-sticky` feel as
snappy as `crosspoint-reader` during focus movement through menus, buttons, and
long lists.

The implementation reference for this plan is the current behavior and code
pattern in:

- `/Users/tieuvong/Development/crosspoint-reader/src/MappedInputManager.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/src/util/ButtonNavigator.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/src/activities/ActivityManager.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/src/activities/home/HomeActivity.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/src/activities/home/FileBrowserActivity.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/src/components/themes/BaseTheme.cpp`
- `/Users/tieuvong/Development/crosspoint-reader/lib/GfxRenderer/GfxRenderer.h`
- `/Users/tieuvong/Development/crosspoint-reader/lib/hal/HalDisplay.h`
- `/Users/tieuvong/Development/crosspoint-reader/lib/hal/HalDisplay.cpp`

The target interaction pattern is:

1. Input changes focus locally and immediately.
2. Focus state is cheap and screen-owned on the hot path.
3. One focus move produces one paint request.
4. Pure focus motion repaints only the old and new visual focus bounds.
5. The panel refresh path stays as short as possible.

## Phase Rules

A phase is not complete until:

- every requirement in that phase is checked off
- you run a build and report whether it is clean
- I re-evaluate the implementation against the `crosspoint-reader` reference
  files above and confirm the phase still matches the intended logic pattern
- you explicitly give the go-ahead to close the phase

Until then, leave the phase completion box unchecked.

## Scope

This plan covers app/runtime focus handling, focus-driven repaint behavior,
refresh dispatch, and final parity checks against `crosspoint-reader`.

Out of scope:

- board wiring changes
- touch or panel driver rewrites
- low-level e-paper command sequencing redesign
- unrelated product feature work

If any phase appears to require driver-level changes, stop and record that gap
before continuing.

## Tracking Rules

- [ ] Keep this document updated as the source of truth for phase status.
- [ ] Do not check off a phase completion box until build, crosspoint-pattern
  review, and your approval are all done.

## Phase 1: Local Focus First

Goal: make page focus movement follow the `crosspoint-reader` hot path where a
screen-owned focus index changes immediately on input before any heavier
coordination happens.

### Requirements

- [x] Inventory every current focusable screen in `folloup-sticky` and mark
  whether it should use the shared page-focus path, footer path, or overlay
  path.
- [x] For page-owned screens, make local page focus the hot-path truth for
  button navigation, matching the spirit of `selectorIndex` updates in
  `crosspoint-reader` `HomeActivity` and `FileBrowserActivity`.
- [x] Ensure a button focus move does not require service reads, full page
  rebuild decisions, or unrelated cross-surface work before the local focus
  state changes.
- [x] Align hold behavior with the `crosspoint-reader` navigation pattern from
  `ButtonNavigator.cpp`: predictable first repeat, predictable repeat interval,
  and no accidental double-trigger on release after hold-repeat.
- [x] For long lists, define where hold-repeat should page-jump instead of
  stepping one row at a time, following the `FileBrowserActivity` pattern.
- [x] Preserve overlay-first, then footer, then page input precedence while
  still keeping the page-owned focus mutation path short.
- [x] Prevent secondary touch-only selection state from appearing on any page;
  touch-down focus and button-driven focus must point at the same focus truth.
- [x] Update `docs/app-architecture.md` once the final focus ownership for this
  phase is real.

### Current Inventory

| Surface | Focus path | Notes |
| --- | --- | --- |
| `Home` screen | Footer path | No page-owned focus provider is registered today; footer owns the interactive focusable targets on this surface. |
| `Settings` screen | Shared page-focus path | `SettingsPageCoordinator` owns the hot-path focus index and projects footer focus from page focus. |
| `WiFi` screen | Shared page-focus path | `WifiPageCoordinator` owns page focus; the network list keeps a page-owned sub-focus for long-list interaction. |
| `Lock screen` | None today | No roving focus surface is exposed here outside global button shortcuts. |
| Shutdown modal | Overlay path | Overlay-owned roving focus. |
| Storage modal | Overlay path | Overlay-owned modal actions. |
| Select modal | Overlay path | Overlay-owned roving focus. |
| Keyboard overlay | Overlay path | Overlay-owned key focus and submit/dismiss flow. |
| Toast close action | Overlay path | Overlay-owned close affordance when visible. |

### Phase 1 Progress Notes

- Footer touch precedence is unchanged, but page-owned screens no longer create
  a temporary footer-only focus state on touch-down before syncing page focus
  afterward.
- `Settings` and `WiFi` now map footer touch focus directly into their
  coordinator-owned focus truth, then project the footer from that page-owned
  focus.
- Shared navigation hold timing now has an explicit first-repeat gate so the
  hot path does not depend purely on raw callback cadence.
- While the WiFi network list is active, button press-down still steps one row
  at a time, and hold-repeat now page-jumps by the current visible row
  capacity to match the `crosspoint-reader` long-list pattern.

### Validation

- [x] Build completed successfully for Phase 1.
- [ ] Button focus movement on migrated screens follows the intended
  `crosspoint-reader` pattern of local state mutation first, then refresh.
- [ ] Hold behavior and release behavior feel consistent with the reference
  `ButtonNavigator.cpp` flow.
- [x] After the clean build, I re-evaluated the current implementation against
  the `crosspoint-reader` input and focus references and confirmed coverage.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 1 complete.

## Phase 2: Focus Bounds Only

Goal: change focus-driven repaint from coarse page refreshes to visual-bounds
repaint rules so pure focus motion repaints only what changed.

### Requirements

- [x] Add component-owned visual bounds helpers for every focusable control
  whose focus styling extends beyond its nominal layout box.
- [x] For pure focus moves, compute the repaint region as the union of the old
  focused visual bounds and the new focused visual bounds.
- [x] Ensure focus repaint clears old focus styling and paints new focus
  styling across full visual bounds, including rings, gaps, rounded corners,
  shadows, or highlighted value pills.
- [x] Route pure focus-motion refreshes through region partial refresh instead
  of whole-screen partial refresh whenever the visual change is bounded.
- [x] Keep larger refresh triggers for page entry, page identity changes, list
  viewport changes, large content reloads, sleep/wake recovery, and forced full
  refreshes.
- [x] Preserve the stricter overlay rule already present in
  `folloup-sticky`: overlay visibility changes or underlay restoration paths
  must rebuild correctly rather than using a too-small dirty region.
- [x] Document the redraw-scope categories introduced in this phase and where
  that scope decision lives.

### Phase 2 Progress Notes

- `epaper_ui` now exposes component-owned visual-bounds helpers for the current
  focusable `Settings` and `WiFi` controls, including footer items, the WiFi
  password field, and the active network-list row.
- Pure focus updates now compute a dirty region as the union of old and new
  visual focus bounds instead of blindly repainting the whole screen.
- `page_actions::FocusUpdateOutcome` and `FocusMoveOutcome` now carry bounded
  repaint metadata so the page input path can distinguish focus-only region
  repaint from broader page refreshes.
- `display_service::RefreshRequest` now carries refresh mode plus refresh
  scope, and `ui_refresh_runtime` preserves that scope while coalescing
  latest-wins page refreshes.
- When no overlay is visible, `display_service` may execute a raw-coordinate
  region partial refresh for bounded focus motion on `Settings` and `WiFi`.
- Overlay-visible refreshes still intentionally fall back to the safer broader
  underlay rebuild path instead of attempting too-small dirty-region restores.
- WiFi network-list viewport changes remain a larger repaint case: focus motion
  inside the list uses a bounded row union when the viewport is stable, but it
  falls back to the full list region when scrolling changes the visible window.

### Validation

- [x] Build completed successfully for Phase 2.
- [ ] Rapid focus changes do not leave stale pixels behind in any migrated
  control family.
- [x] Pure focus motion no longer falls back to full-screen partial refresh
  unless the control family explicitly requires it.
- [x] After the clean build, I re-evaluated the current repaint logic against
  the `crosspoint-reader` pattern of “minimal work for focus motion” and
  confirmed the intended behavior is covered.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 2 complete.

## Phase 3: Fast Refresh Lane

Goal: keep the existing freshness-biased architecture where it helps, but make
focus updates travel through the shortest possible render and panel path.

### Requirements

- [x] Define a refresh-scope model that distinguishes focus-only region repaint,
  bounded page repaint, whole-screen partial refresh, and full base refresh.
- [x] Keep a latest-wins presentation path for stale update coalescing, but
  ensure pure focus-motion updates do not rebuild more page state than needed.
- [x] Remove direct display work from input callback paths while still allowing
  focus updates to reach the panel through the shortest safe route.
- [x] Carry dirty-region and refresh-scope information through the UI refresh
  path so display execution can choose region partial vs full-screen partial.
- [x] Keep synchronous page-entry routing outside the latest-wins queue when
  deterministic screen transition ordering matters.
- [x] Ensure footer sync only triggers repaint when footer-visible focus state
  actually changed.
- [ ] Reuse retained underlay or retained page content where appropriate so the
  renderer restores unchanged pixels instead of rebuilding the whole visible
  page on every focus move.
- [x] Update architecture docs to describe the final refresh ownership and
  focus fast-lane accurately.

### Phase 3 Progress Notes

- `ui_refresh_runtime` is now the single latest-wins queue for page, footer,
  status, lock-screen, and overlay refresh scheduling; the older app-shell
  `ui_refresh_dispatcher` hop has been removed.
- `Settings` and `WiFi` page focus updates now queue one async composite apply
  only when the visible footer projection actually changes; otherwise they only
  queue the page-owned state apply.
- Page-owned focus updates still carry dirty-region and refresh-scope metadata
  through `ui_refresh_runtime` into `display_service`, but they no longer rely
  on per-page app-shell refresh handlers to reach the display queue.
- Synchronous page-entry routing remains outside the latest-wins queue in
  `app_shell` so screen transitions still set up touch provider, footer layout,
  page state, and current screen in deterministic order.
- Footer projection sync is now computed from old vs new page focus projection
  instead of being treated as an automatic side effect of every page focus
  change.

### Validation

- [x] Build completed successfully for Phase 3.
- [ ] Repeated focus moves do not block on unnecessary page rebuild or display
  queue churn.
- [ ] Stale intermediate focus updates are coalesced correctly while the panel
  is busy.
- [x] After the clean build, I re-evaluated the render and refresh path against
  the `crosspoint-reader` reference chain of “focus move -> one paint request ->
  fast refresh” and confirmed the pattern is covered.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 3 complete.

## Phase 4: Crosspoint Parity Sweep

Goal: confirm the repo now covers the intended `crosspoint-reader` logic and
interaction pattern closely enough to close the focus refactor.

### Requirements

- [x] Re-compare every migrated focusable screen against the referenced
  `crosspoint-reader` logic for input mapping, hold behavior, local focus
  mutation, and refresh triggering.
- [x] Verify long-list behavior against the `crosspoint-reader` list/menu
  pattern and document any intentional deviations.
- [x] Verify the final panel refresh mode choices still match the intent of
  this plan and record any screen families that must remain coarser than the
  reference pattern.
- [x] Remove or document any transitional refresh plumbing, duplicate routing,
  or stale focus-projection work that is no longer needed.
- [x] Update `docs/app-architecture.md` and any adjacent docs so they describe
  actual runtime behavior rather than transition scaffolding.
- [x] Record every intentional deviation from `crosspoint-reader` and the
  product or hardware reason it remains.

### Phase 4 Progress Notes

- `Settings` page parity check: button focus mutation is page-owned and local
  before refresh scheduling, matching the `HomeActivity` pattern of mutating
  `selectorIndex` first and requesting one update afterward.
- `WiFi` page parity check: active network-list hold-repeat now follows the
  `FileBrowserActivity` page-jump pattern, while single press-down still steps
  by one row for precise selection.
- Shared hold behavior parity check: `navigation_input_controller` now mirrors
  the `ButtonNavigator` shape of press-driven navigation plus gated continuous
  repeats, and it suppresses the release-side double-trigger after repeats.
- Refresh-lane parity check: pure focus motion now routes through one latest-
  wins queue and one display refresh request rather than the earlier
  `ui_refresh_runtime` plus app-shell dispatcher double hop.
- Transitional refresh scaffolding removed in this phase: page/footer refresh
  handlers in `app_shell` and the separate `ui_refresh_dispatcher` worker.

### Current Intentional Deviations

- Overlay precedence remains stricter than `crosspoint-reader` because retained
  modal and keyboard overlays must trap input and may force broader underlay
  rebuilds to preserve e-paper correctness.
- Synchronous page-entry routing remains in `app_shell` rather than the latest-
  wins queue because this firmware must order touch-provider setup, footer
  layout, runtime state sync, and screen switch deterministically.
- Overlay-visible focus churn still falls back to the safer broader refresh
  path instead of region refresh because retained overlay restoration is more
  important than matching the narrowest possible repaint scope.
- WiFi list viewport changes still repaint the whole list region when the
  visible window scrolls, because the content shift is larger than a simple
  old/new focus highlight swap.

### Validation

- [x] Build completed successfully for Phase 4.
- [ ] End-to-end focus behavior has been re-checked against the
  `crosspoint-reader` reference and no untracked logic gaps remain.
- [ ] Remaining differences, if any, are intentional and documented.
- [x] After the clean build, I performed the final `crosspoint-reader`
  re-evaluation for this phase and confirmed whether the logic pattern is fully
  covered.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 4 complete.

## Exit Criteria

- [ ] All phase completion boxes are checked.
- [ ] The build has passed for every completed phase.
- [ ] I re-evaluated the `crosspoint-reader` reference after each clean build
  before any phase was closed.
- [ ] Every remaining deviation from the reference pattern is explicit and
  accepted.
- [ ] You approved the final state.
