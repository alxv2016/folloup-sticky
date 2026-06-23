# Followup Input Alignment Plan

This document tracks the refactor work needed to align this repo's input and
input-driven refresh strategy with the current `followup` implementation.

It is intentionally phase-based. A phase is not complete until:

- every requirement in that phase is checked off
- you run a build and we record the result
- we validate the runtime behavior against `followup`
- you explicitly give the go-ahead to close the phase

Until then, leave the phase completion box unchecked.

## Scope

The target alignment is based on the current `followup` input and refresh
strategy, especially:

- repo-wide page focus and activation handling
- shared overlay/footer/page input precedence
- shared latest-wins UI presentation dispatch
- page-state snapshotting before presentation
- refresh behavior that distinguishes redraw scope from panel refresh scope
- visual-bounds-aware focus repaint behavior

## Current Gaps

- This repo only has the newer page input model wired for `Settings` and
  `WiFi`; `followup` applies it across the focusable page set.
- This repo still splits UI work across `ui_refresh_runtime` and
  `ui_refresh_dispatcher`; `followup` uses one shared freshness-biased
  presentation lane for pages and overlays.
- This repo keeps more page activation and service mutation logic inside page
  runtimes; `followup` separates more of that behavior into interaction layers
  with clearer intent/result boundaries.
- This repo does not yet have `followup`'s explicit redraw-scope planning and
  visual-bounds refresh rules for focus styling.
- Cross-surface page/footer coordination is more manual here than in
  `followup`'s shared snapshot/presentation flow.

## Tracking Rules

- [x] Gap analysis captured and translated into a working refactor plan.
- [ ] Keep this document updated as the source of truth for phase status.
- [ ] Do not check off a phase completion item until build, behavior
  validation, and your approval are all done.

## Phase 1: Expand Shared Input Coverage

Goal: move from a `Settings`/`WiFi`-only shared input path to a repo-wide input
architecture that matches `followup`'s page coverage.

### Requirements

- [ ] Inventory every current and planned focusable screen in this repo and map
  each one to its target input owner.
- [ ] Define the page-by-page migration order from current screen-specific input
  handling to shared page focus handling.
- [ ] Extend shared button focus routing beyond `Settings` and `WiFi` so active
  page focus movement is not hard-coded to only those screens.
- [ ] Extend the shared touch contract beyond `Settings` and `WiFi` so page
  touch providers are not limited to those two screens.
- [ ] Preserve overlay-first, then footer, then page precedence during the
  migration.
- [ ] Preserve the current touch lifecycle: touch-down focuses immediately,
  touch-move may retarget, touch-up activates only the armed target.
- [ ] Keep `app_shell` as orchestration-only while moving page-specific input
  decisions into focused helpers.
- [ ] Update `docs/app-architecture.md` once the repo-wide ownership model is
  real, not aspirational.

### Validation

- [ ] Build completed successfully for Phase 1.
- [ ] Button focus movement works on every migrated page and matches
  `followup`'s shared navigation behavior.
- [ ] Touch precedence logs and behavior match overlay, footer, then page
  ownership across every migrated page.
- [ ] No page introduces a second touch-only selection state outside the shared
  focus truth model.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 1 complete.

## Phase 2: Separate Page Interaction Semantics From Page Runtime State

Goal: align page interaction structure with `followup` so page runtimes own
state and rendering inputs, while interaction helpers own focus/activate
semantics and intent shaping.

### Requirements

- [ ] Define the target split between coordinator/runtime state,
  interaction-intent helpers, and app-level orchestration callbacks.
- [ ] Move page activation semantics out of runtime-heavy code paths where the
  behavior belongs in focused interaction helpers.
- [ ] Normalize per-page focus-move result shapes so pages can report handled
  state, refresh needs, feedback cues, and follow-on intents consistently.
- [ ] Normalize per-page activate result shapes so page activation can trigger
  navigation, modal opens, service actions, or page-local changes through a
  common pattern.
- [ ] Normalize footer activation handling so page modules do not re-implement
  divergent footer action rules.
- [ ] Keep page-local state construction inside page/coordinator code rather
  than moving service reads into rendering primitives.
- [ ] Keep product feedback mapping centralized so page helpers emit neutral
  cues instead of directly owning buzzer policy.

### Validation

- [ ] Build completed successfully for Phase 2.
- [ ] Migrated pages expose interaction behavior through consistent helper
  contracts rather than bespoke runtime branches.
- [ ] Activation behavior matches `followup` for the equivalent pages and
  controls.
- [ ] Feedback behavior still routes through app-owned policy rather than page
  helpers directly.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 2 complete.

## Phase 3: Unify UI Presentation Dispatch

Goal: replace the current split between `ui_refresh_runtime` and
`ui_refresh_dispatcher` with one shared freshness-biased presentation model that
matches `followup`.

### Requirements

- [ ] Document the target end state for shared UI presentation ownership before
  changing the implementation.
- [ ] Consolidate page and overlay presentation work onto one shared latest-wins
  dispatch path.
- [ ] Preserve keyed coalescing so stale intermediate updates are dropped while
  the panel is busy.
- [ ] Move page-state capture onto the shared refresh worker so capture and
  presentation are consistently decoupled from input/service callback tasks.
- [ ] Ensure the worker captures the latest state under the owning mutex, then
  releases the mutex before presenting to `display_service`.
- [ ] Keep synchronous page-entry routing outside the freshness-biased lane so
  page transition ordering stays deterministic.
- [ ] Route `Settings` and `WiFi` through the same shared model used for the
  rest of the migrated pages.
- [ ] Route repeated overlay churn through that same freshness-biased model.
- [ ] Remove transitional refresh plumbing once the replacement path is proven.

### Validation

- [ ] Build completed successfully for Phase 3.
- [ ] Repeated focus moves do not block on direct display work from the input
  callback path.
- [ ] Stale page and overlay updates are coalesced the way `followup` expects.
- [ ] Mutex hold time around page-state capture is measurably shorter or at
  least structurally aligned with the `followup` model.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 3 complete.

## Phase 4: Align Refresh Planning And Visual Bounds Rules

Goal: port the missing refresh-planning behavior that `followup` uses for
input-driven focus updates and overlay interactions.

### Requirements

- [ ] Define the redraw-scope model this repo needs to support, based on the
  current `followup` categories.
- [ ] Identify the current framebuffer or retained-view boundaries where redraw
  scope classification should live.
- [ ] Add component-owned visual bounds helpers for controls whose focused
  styling extends beyond nominal layout bounds.
- [ ] Ensure focus moves erase old focused styling and paint new focused styling
  across the full visual bounds, not just nominal control boxes.
- [ ] Keep normal active-page updates on the same panel refresh mode that
  `followup` uses unless hardware evidence forces a different choice.
- [ ] Preserve larger rebuild triggers for page entry, page identity changes,
  major content reloads, overlay visibility transitions, sleep/wake recovery,
  and forced full refreshes.
- [ ] Preserve the stricter overlay rule: visibility changes and underlay
  restoration paths must rebuild correctly.
- [ ] Update architecture and refresh documentation once the actual runtime rule
  is implemented.

### Validation

- [ ] Build completed successfully for Phase 4.
- [ ] Focus rings, gaps, rounded corners, and outlined content do not leave
  stale pixels during rapid focus changes.
- [ ] Overlay show/hide and same-visibility churn behave like `followup`
  without underlay corruption.
- [ ] Panel refresh mode choices and redraw scope classification match the
  intended `followup` behavior on-device.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 4 complete.

## Phase 5: Align Service-Driven UI Updates

Goal: make service-originated UI updates use the same shared freshness-biased
presentation model as input-driven page changes.

### Requirements

- [ ] Inventory every service callback that can mutate visible page or overlay
  state.
- [ ] Route service-driven page refreshes through the shared latest-wins
  presentation path.
- [ ] Route service-driven overlay churn through the shared latest-wins
  presentation path.
- [ ] Ensure repeated service updates do not queue stale page snapshots behind a
  busy panel.
- [ ] Keep page-local refresh entry points narrow so service code does not take
  over page composition ownership.
- [ ] Preserve screen-active guards so inactive pages are not pointlessly
  presented.

### Validation

- [ ] Build completed successfully for Phase 5.
- [ ] Repeated service updates coalesce correctly instead of flooding the
  display path.
- [ ] Active-page and active-overlay updates match `followup`'s freshness
  behavior.
- [ ] Inactive pages do not perform unnecessary presentation work.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 5 complete.

## Phase 6: Final Alignment Sweep

Goal: close remaining deltas, verify docs, and confirm this repo is aligned
enough with `followup` to treat the refactor as done.

### Requirements

- [ ] Re-compare the final implementation against `followup`'s current input
  and UI refresh behavior.
- [ ] Audit for leftover transitional paths, duplicated dispatchers, or
  screen-specific exceptions that no longer need to exist.
- [ ] Update `docs/app-architecture.md` to describe the final input and refresh
  ownership accurately.
- [ ] Update any adjacent docs touched by the refactor so they describe actual
  runtime behavior, not interim scaffolding.
- [ ] Capture any intentional deviations from `followup` and the reason they
  remain.

### Validation

- [ ] Build completed successfully for Phase 6.
- [ ] End-to-end runtime behavior is validated against the intended `followup`
  model.
- [ ] Remaining differences, if any, are intentional and documented.
- [ ] You reviewed the result and gave the go-ahead.

### Phase Completion

- [ ] Phase 6 complete.

## Exit Criteria

- [ ] All phase completion boxes are checked.
- [ ] The build has passed for every completed phase.
- [ ] On-device or equivalent runtime validation has been recorded for every
  phase.
- [ ] This repo's input strategy is fully aligned with `followup`, or every
  remaining deviation is explicitly accepted and documented.
- [ ] You approved the final state.
