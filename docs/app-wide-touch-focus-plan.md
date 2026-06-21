# App-Wide Touch Focus And Activation Plan

This document tracks the Sticky port of an application-wide touch interaction
model where touching an interactive target focuses it immediately and activation
is handled as a separate step.

This work is not overlay-specific. The goal is to build the shared interaction
path for:

- overlays
- the global footer
- future page-level interactive elements
- future composite controls such as lists and segmented areas

This is a phased port. We do not start the next phase until:

1. every checklist item in the current phase is complete
2. the user runs a build
3. the user explicitly confirms the build is clean

Update this checklist as work completes. Treat it as the source of truth for
this port.

## Goals

- Make touch-down focus the touched interactive target immediately.
- Keep activation separate from focus so touch and button interaction models
  follow the same architecture.
- Build this once at the application level instead of adding overlay-only touch
  behavior that would need to be extracted later.
- Preserve the rule that runtime state mutates first and display refresh catches
  up through the keyed latest-wins refresh lane.
- Keep `main/app_shell.cpp` as an orchestration layer.

## Shared Interaction Rule

The intended app-wide touch model is:

- touch-down on an interactive target focuses it immediately
- touch-move may retarget focus to a different interactive target
- touch-up activates only if the release still matches the armed target
- touch-up off-target cancels activation while keeping the latest focused state

This gives visible focus on touch without requiring a second tap and keeps
activation semantics distinct from focus semantics.

## Architecture Rules

These rules come from Sticky's current app architecture and apply to this port:

- `app_shell` may wire touch and button events into the shared interaction
  runtime, but it must not become the home for touch contact state, target
  resolution policy, or widget-specific activation rules
- `epaper_ui` stays render-only and may expose geometry and hit-test helpers,
  but it must not own retained focus truth or input policy
- reusable navigation primitives belong in `components/`, not in `app_shell`
- app-specific target identity and app-level input precedence belong under
  `main/`
- retained overlay focus state stays in `overlay_runtime`
- footer/page focus state stays with the runtime or coordinator that owns that
  surface
- focus mutation and activation decisions must remain separate from synchronous
  display work

## Ownership Direction

The intended ownership split for this port is:

- `components/page_navigation/`
  - reusable focus/navigation primitives
  - any reusable non-app-specific interaction helpers that prove broadly useful
- `main/`
  - shared app-level touch interaction runtime
  - app-specific interactive target descriptors
  - app-wide input precedence and routing policy
- `overlay_runtime`
  - retained overlay state
  - focus/activate hooks for shutdown/select/toast targets
- `footer_runtime`
  - projection of focused footer target into footer UI state
- future page runtimes/coordinators
  - page-local focus truth and page target activation hooks
- `ui_refresh_runtime`
  - asynchronous latest-wins presentation scheduling

## Not In Scope For This Port

- full page navigation for every future page
- keyboard overlay interaction parity
- gesture recognition beyond touch-down, move, and release
- drag/scroll physics for future list views
- rewriting `epaper_ui` render contracts that already express selected/focused
  visuals correctly

Those can build on this shared touch architecture later.

## Phase Gate Rule

Each phase stays locked until all of these are true:

- [ ] Every requirement in the phase is complete
- [ ] The user ran a build
- [ ] The user confirmed the build is clean
- [ ] We explicitly marked the phase complete in this document

## Phase 1: Shared Interaction Contract

Objective:
Define the app-wide touch interaction contract and the shared target model
before moving behavior into runtime code.

Requirements:

- [x] Document the app-wide touch lifecycle:
      - touch-down focuses
      - touch-move may retarget focus
      - touch-up activates only on the armed target
      - touch-up off-target cancels activation
- [x] Define an app-specific interactive target descriptor for all app-owned
      focusable elements
- [x] Confirm the target model can represent:
      - select modal items
      - shutdown modal actions
      - toast close action
      - footer items
      - future page actions
      - future page list rows/composites
- [x] Keep the target descriptor out of `epaper_ui`
- [x] Document which runtime owns focus truth for each current surface
- [x] Update this checklist when the shared interaction contract is settled

Phase 1 completion:

- [x] Phase 1 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 1 marked complete

## Phase 2: Touch Event Lifecycle Delivery

Objective:
Make touch release and continued contact first-class app events so the shared
interaction runtime can separate focus from activation.

Requirements:

- [x] Ensure app-level touch handling receives enough signal to distinguish:
      - new contact
      - continued contact
      - release/no contact
- [x] Stop treating touch as a contact-only signal for interactive elements
- [x] Preserve current auto-sleep user-activity signaling
- [x] Preserve current touch feedback behavior where it still makes sense
- [x] Keep this lifecycle work app-owned rather than burying policy inside the
      generic GT911 driver
- [x] Update this checklist when release-aware touch delivery is in place

Phase 2 completion:

- [x] Phase 2 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 2 marked complete

## Phase 3: Shared App Interaction Runtime

Objective:
Introduce a shared runtime under `main/` that owns app-wide touch focus and
activation policy instead of scattering tap semantics across individual
surfaces.

Requirements:

- [x] Add a shared app-owned interaction runtime under `main/`
- [x] Keep `app_shell` limited to forwarding button/touch events into that
      runtime
- [x] Make the runtime own retained touch contact state such as:
      - contact active
      - armed target
      - latest focused target
- [x] Define app-wide input precedence for touch target resolution:
      - overlays first
      - footer/page targets after overlays
      - future page composites under the same contract
- [x] Keep target resolution separate from target activation
- [x] Keep this runtime as the home for app-specific input policy rather than
      putting it into `overlay_runtime`
- [x] Update this checklist when the shared interaction runtime exists

Phase 3 completion:

- [x] Phase 3 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 3 marked complete

## Phase 4: Overlay Focus-Then-Activate Migration

Objective:
Move current overlay touch behavior onto the shared touch contract so modal
touches focus first and activate on release instead of submitting immediately on
contact.

Requirements:

- [x] Refactor select modal touch handling to:
      - focus the touched item immediately
      - keep roving focus as the single source of truth
      - submit only on valid release/activation
- [x] Refactor shutdown modal touch handling to:
      - focus the touched button immediately
      - activate only on valid release/activation
- [x] Keep overlay retained state ownership inside `overlay_runtime`
- [x] Preserve overlay-first input capture precedence
- [x] Keep button activation behavior aligned with touch activation behavior
- [x] Rework any touch cooldown logic so it guards repeated activation rather
      than blocking normal focus updates during contact
- [x] Update this checklist when overlays follow the shared touch contract

Phase 4 completion:

- [x] Phase 4 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 4 marked complete

## Phase 5: Footer Integration

Objective:
Make the global footer participate in the same touch focus and activation model
without introducing a second independent interaction system.

Requirements:

- [x] Add footer interactive target resolution under the shared interaction
      runtime
- [x] Keep footer selection as a projection of shared focus truth rather than a
      separate standalone footer touch state
- [x] Ensure tapping a footer item focuses it immediately
- [x] Ensure footer activation follows the same release-on-armed-target rule
- [x] Preserve the current mic active visual behavior while adding focus-on-tap
- [x] Update this checklist when the footer participates in the shared touch
      contract

Phase 5 completion:

- [x] Phase 5 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 5 marked complete

## Phase 6: Page Integration Groundwork

Objective:
Prepare the shared interaction runtime so future page ports can plug into the
same focus/activate contract instead of building page-specific tap models.

Requirements:

- [x] Define the contract future page runtimes/coordinators will implement for:
      - resolve touched target
      - focus touched target
      - activate focused target
- [x] Ensure page-local selected indexes remain projections of page-owned focus
      truth rather than separate touch-only state
- [x] Ensure composite page controls can participate later without redesigning
      the shared interaction runtime
- [x] Document how footer selection and page-body selection can still derive
      from shared focus truth where applicable
- [x] Update this checklist when the page integration contract is documented

Phase 6 completion:

- [x] Phase 6 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 6 marked complete

## Phase 7: Refresh, Logging, And Validation

Objective:
Close the port with the correct refresh behavior, observability, and validation
notes so future interaction work reuses the same contract.

Requirements:

- [x] Ensure focus mutation still happens before display refresh
- [x] Route focus and activation presentation through `ui_refresh_runtime`
- [x] Ensure stale intermediate touch-focus states are discarded while the
      panel is busy
- [x] Add focused logs for:
      - target resolved
      - focus changed
      - activation committed
      - activation canceled
      - precedence path selected
- [x] Document remaining hardware validation for:
      - touch-down focus visibility
      - release-based activation behavior
      - overlay/footer precedence
      - behavior across display sleep/light-sleep wake
- [x] Update architecture-facing docs after implementation settles
- [x] Update this checklist to reflect final completion state

Phase 7 completion:

- [x] Phase 7 requirements complete
- [x] User ran a build
- [x] User confirmed build is clean
- [x] Phase 7 marked complete

## Final Completion Gate

We consider this port complete only when:

- [x] All phase checklists are complete
- [x] All phase build gates have been satisfied
- [x] The user confirms the final build is clean
- [x] Any remaining known gaps are documented explicitly
