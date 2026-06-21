# Focus, Roving, And Refresh Port Plan

This document tracks the Sticky port of the Followup interaction model for:

- shared roving focus
- overlay focus traps
- decoupled UI refresh
- footer/page focus projection groundwork

This is a phased port. We do not start the next phase until:

1. every checklist item in the current phase is complete
2. the user runs a build
3. the user explicitly confirms the build is clean

Update this checklist as work completes. Treat it as the source of truth for
port progress.

## Goals

- Port Followup's shared roving-focus behavior instead of keeping modal-local
  index math scattered through runtime code.
- Keep `main/app_shell.cpp` as an orchestration layer.
- Keep `components/epaper_ui/` render-only.
- Make focus movement immediate in runtime state even when the e-paper panel is
  still busy.
- Present only the latest focus state when refreshes pile up behind a slow
  panel refresh.
- Prepare the global footer to participate in shared focus projection when more
  pages are ported.

## Architecture Rules

These rules come from Sticky's current app architecture and apply to this port:

- `app_shell` may wire events and compose policy, but it must not become the
  home for roving-index ownership, display drawing logic, or long-lived
  interaction state machines.
- reusable focus/navigation primitives belong in a component, not in
  `app_shell`
- app-owned input routing belongs in a focused runtime helper under `main/`
- `overlay_runtime` should own retained overlay state, show/dismiss behavior,
  hit-testing, and display synchronization hooks
- `epaper_ui` stays presentation-only and should only render the state it is
  given
- display refresh scheduling must stay separate from focus-state mutation

## Followup Behaviors Being Ported

- `RovingFocus` wraparound movement is shared and reusable.
- overlay focus takes priority over page focus
- select modal focus movement is separate from submit/dismiss behavior
- page/footer selection is derived from one focus source of truth
- input/focus movement is detached from synchronous display presentation
- UI refresh is freshness-biased and latest-wins while the panel is busy

## Not In Scope For This Port

- full page navigation model for all future Sticky pages
- keyboard overlay port
- toast close-button focus model
- full page-state projection across dashboard/settings/files/time/wifi pages

Those can build on this work later once the shared primitives and refresh lane
exist.

## Phase Gate Rule

Each phase stays locked until all of these are true:

- [ ] Every requirement in the phase is complete
- [ ] The user ran a build
- [ ] The user confirmed the build is clean
- [ ] We explicitly marked the phase complete in this document

## Phase 1: Shared Roving Focus Primitive

Objective:
Port the reusable roving-focus primitive into Sticky as a component-level
building block with no display or modal ownership baked into it.

Requirements:

- [ ] Create a reusable navigation component for Sticky instead of keeping
      wraparound logic inside `overlay_runtime`
- [ ] Port a `RovingFocus` primitive with:
      - configurable item count
      - configurable initial index
      - wraparound move behavior
      - direct index set behavior
- [ ] Keep the primitive independent from:
      - select modal implementation
      - shutdown modal implementation
      - display refresh logic
      - `app_shell`
- [ ] Document the primitive's source of truth and intended ownership boundary
- [ ] Confirm modal behavior still targets wraparound semantics:
      - last item + `DOWN` -> first item
      - first item + `UP` -> last item
- [ ] Update this document when the primitive is in place

Phase 1 completion:

- [ ] Phase 1 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 1 marked complete

## Phase 2: Sticky Input And Focus Runtime

Objective:
Introduce a focused runtime helper under `main/` that owns input precedence and
focus routing without bloating `app_shell`.

Requirements:

- [ ] Add a focused runtime helper for app-owned input/focus routing
- [ ] Keep `app_shell` limited to wiring button/touch events into that helper
- [ ] Define Sticky's current focus precedence:
      - select modal
      - shutdown modal
      - future overlay focusables
      - future page focus
- [ ] Route `UP` and `DOWN` movement through the shared focus helper instead of
      modal-local math
- [ ] Play navigation feedback only when focus actually changes
- [ ] Ensure the focus runtime can request UI presentation updates without
      performing heavy synchronous display work inline
- [ ] Update this document when routing ownership has moved out of
      `overlay_runtime`

Phase 2 completion:

- [ ] Phase 2 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 2 marked complete

## Phase 3: Modal Integration

Objective:
Move Sticky's modal roving behavior onto the shared navigation primitive and
runtime routing layer.

Requirements:

- [ ] Refactor select modal button navigation to use shared roving focus
- [ ] Refactor shutdown modal button navigation to use shared roving focus
- [ ] Keep modal state ownership in `overlay_runtime`
- [ ] Keep submit/dismiss behavior separate from focus movement
- [ ] Preserve Sticky-specific select-modal behavior:
      - `POWER_OK` submits the focused item
      - `DOWN` double-click submits the focused item
      - touch updates focus to the touched item and submits
- [ ] Preserve overlay focus trap behavior so page focus does not move while a
      modal is active
- [ ] Verify touch and button paths cannot drift to different selected indexes
- [ ] Update this document when modal navigation no longer depends on
      duplicated local index math

Phase 3 completion:

- [ ] Phase 3 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 3 marked complete

## Phase 4: Decoupled UI Refresh Lane

Objective:
Port Followup's distinct input-to-refresh behavior so focus moves immediately in
runtime state and the display catches up through a freshness-biased UI refresh
lane.

Requirements:

- [ ] Introduce a shared UI refresh dispatcher/runtime worker for Sticky
- [ ] Keep input/focus mutation separate from display presentation
- [ ] Ensure focus movement updates runtime state immediately
- [ ] Ensure presentation is scheduled asynchronously instead of executed inline
      from input handlers
- [ ] Make queued UI refreshes latest-wins for keyed surfaces while the panel is
      busy
- [ ] Ensure stale intermediate focus states are discarded when newer focus
      states arrive
- [ ] Keep input handling higher priority than presentation work
- [ ] Keep refresh scheduling out of `app_shell` business logic where possible
- [ ] Document which surfaces are keyed at minimum:
      - overlay state
      - active page/footer state
- [ ] Update this document when focus movement is detached from synchronous
      display refresh

Phase 4 completion:

- [ ] Phase 4 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 4 marked complete

## Phase 5: Overlay Refresh Policy

Objective:
Adopt the Followup rule that overlay visibility transitions and same-visibility
overlay churn are presentation concerns distinct from input movement.

Requirements:

- [ ] Keep overlay show/hide transitions on the stronger underlay cleanup path
- [ ] Avoid forcing full local logic rebuilds for every same-visibility overlay
      focus move
- [ ] Ensure repeated select-modal focus movement uses the new refresh lane
- [ ] Ensure repeated toast updates use the new refresh lane
- [ ] Ensure overlay presentation remains correct when underlay focus styling is
      suppressed
- [ ] Confirm the implementation preserves Sticky's current shutdown/select/toast
      behavior while improving responsiveness
- [ ] Update this document when overlay presentation is running through the
      shared refresh lane

Phase 5 completion:

- [ ] Phase 5 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 5 marked complete

## Phase 6: Footer Focus Projection Groundwork

Objective:
Prepare Sticky's global footer for future shared page focus without introducing
 isolated footer truth that will fight later page ports.

Requirements:

- [ ] Keep the current mic footer behavior working during recording/transcribing
- [ ] Avoid introducing standalone footer-navigation truth that is separate from
      future page focus
- [ ] Define how footer selection will eventually be projected from shared page
      focus
- [ ] Keep footer rendering presentation-only
- [ ] Ensure footer state shape can grow into shared focus projection later
- [ ] Update this document when the footer is structurally ready for future page
      ports

Phase 6 completion:

- [ ] Phase 6 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 6 marked complete

## Phase 7: Documentation And Validation Closeout

Objective:
Close the port with updated local documentation and explicit validation notes.

Requirements:

- [ ] Update architecture-facing docs if folder/component ownership changed
- [ ] Record any new runtime modules introduced under `main/`
- [ ] Record any new reusable components introduced under `components/`
- [ ] Document input precedence and focus ownership for Sticky
- [ ] Document the decoupled refresh rule for Sticky so future page ports reuse
      it instead of bypassing it
- [ ] List any hardware verification still pending after the port
- [ ] Update this checklist to reflect final completion state

Phase 7 completion:

- [ ] Phase 7 requirements complete
- [ ] User ran a build
- [ ] User confirmed build is clean
- [ ] Phase 7 marked complete

## Final Completion Gate

We consider this port complete only when:

- [ ] All phase checklists are complete
- [ ] All phase build gates have been satisfied
- [ ] The user confirms the final build is clean
- [ ] Any remaining known gaps are documented explicitly
