# Followup App Architecture

This project is an ESP-IDF C++17 firmware application for the
[Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97).
The board details are in `docs/waveshare-epaper-hardware-spec.md`.

The codebase began as a port targeting the Seeed reTerminal Sticky, and parts of
this document still describe that lineage where the design rationale carried
over. Where the two boards differ, this document describes the Waveshare
hardware, which is the only target the firmware builds for today. The most
significant differences:

| Area | reTerminal Sticky | Waveshare ESP32-S3-ePaper-3.97 |
| --- | --- | --- |
| Input | GT911 capacitive touch + buttons | Buttons only — no touch controller |
| Microphone | PDM mic (`pdm_mic`, `microphone_service`) | ES8311 codec over I2S (`audio_hal`) |
| Speaker | none | ES8311 + NS4150B amplifier |
| Power | BQ27220 fuel gauge + discrete latch | AXP2101 PMIC (rails, charger, power key) |
| RTC | PCF85063 | PCF85063 |
| IMU | — | QMI8658 6-axis |
| Display bus | SPI2 shared with SD | Dedicated SPI3, no bus sharing |

## Current Scope

The repository is a multi-page ESP-IDF product application (dashboard home,
onboarding, and a set of feature pages plus overlays) built on:

- ESP32-S3 target configuration.
- 16 MB flash configuration.
- 8 MB PSRAM configuration.
- OTA-ready partition layout with rollback enabled.
- A minimal C++ `app_main()`.
- An `axp2101` component: the PMIC driver that owns the rails, charger, battery
  telemetry, and the power-key interrupt stream.
- A ported PCF85063 RTC driver.
- A `qmi8658` component for the 6-axis IMU used by motion wake.
- A `board` component (`waveshare_board`) that owns Waveshare-specific pin
  mapping, PMIC rail bring-up, the shared sensor I2C bus, and the audio codec
  instance.
- A `power_service` component that initializes power hardware and logs a
  diagnostic power/battery/RTC snapshot.
- A `button_service` component that logs app-facing button events through
  Espressif's managed button component.
- An `audio_hal` component wrapping the ES8311 codec for full-duplex 16 kHz
  capture and playback, including the NS4150B power-amp enable.
- A `system_sound_service` component that owns the decoded sound-cue catalog and
  streams cues to the codec.
- A `feedback_service` component that owns app-facing interaction feedback
  policy and maps app events onto sound cues.
- A `playback_service` component that streams a clip to the codec, either from a
  WAV file on SD or straight from the PSRAM chunks `recording_service` holds.
- A `design_tokens` component that owns shared product UI constants such as
  spacing, colors, typography roles, and component sizing.
- An `epaper_ui` component that owns the reusable e-paper presentation
  primitives (status bar, global footer, lock screen, card modal, select modal,
  toast, keyboard, carousel, scroll container, timeline list, sticky note, and
  the many list/menu/input widgets) plus the full page renderers (dashboard,
  onboarding, vibe check, summarize, notes, todos, follow-up, details, settings,
  wifi, time).
- A ported `sd_card` component for SDMMC/FATFS MicroSD access.
- A `storage_service` component that owns app-facing MicroSD mount, format, and
  debug status policy.
- A `wifi_service` component that owns ESP-IDF Wi-Fi station/AP lifecycle,
  saved credentials, scan state, and the backend HTTP routes for setup/status.
- A `timezone_service` component that owns timezone settings, SNTP sync,
  system-time updates, PCF85063 RTC writeback, and backend HTTP routes for time
  settings/runtime state.
- A `gemini_service` component that owns Gemini API key settings precedence,
  backend HTTP routes, and Gemini authentication readiness state.
- A `recording_service` component that owns voice-input recording state,
  pre-roll buffering, PSRAM-backed clips, input-level tracking, and WAV export
  to MicroSD.
- A `recording_session_service` component that owns the press/hold recording
  flow, the start/stop sound cues, the review playback of the take, tag
  selection, and save/transcription orchestration.
- A `recording_archive_service` component that owns the SD recording index
  (listing, metadata, follow-up flags) surfaced by the Notes/Todos/Follow-up
  pages and the sticky-note overlay.
- A `transcription_service` and a `summary_service` component that own the
  Gemini-backed transcription and summary flows respectively (these live in
  their own components, not inside `gemini_service`).
- A ported mono SSD1677 e-paper panel driver. On this board the panel owns a
  dedicated SPI3 bus, so there is no shared-bus serialization to do — the
  Sticky's `shared_bus_service` has no counterpart here.
- A `display_service` component that owns app-facing e-paper bring-up, blank
  screen refresh, display sleep, and light-sleep recovery.
- Staged e-paper asset generation scripts and source PNG/TTF assets for the
  app UI.

- A ported QMI8658 6-axis inertial sensor driver.
- An `imu_service` component that owns app-facing IMU bring-up and sample
  logging, used by motion-based wake.
- A `device_sleep_service` component that owns auto-sleep policy state,
  inactivity timing, app-level blocker checks, and staged sleep events.
- A `task_config` component that owns the app-created FreeRTOS task priority
  and core-affinity mapping.
The board carries an SHTC3 temperature/humidity sensor on the shared sensor I2C
bus. It is not driven by any component today.

This board has no touch controller — input is entirely buttons. Some widget code
still carries `kTouch*` hit-slop constants and a `kTouchContact` feedback cue
inherited from the Sticky port; they are vestigial and nothing dispatches touch
events.

## Project Layout

```text
CMakeLists.txt
assets/
  epaper_assets.json
  icons/
  logos/
fonts/
main/
  CMakeLists.txt
  main.cpp
  app_shell.h
  app_shell.cpp
  button_input_runtime.h
  button_input_runtime.cpp
  device_sleep_runtime.h
  device_sleep_runtime.cpp
  footer_runtime.h
  footer_runtime.cpp
  input_callback_dispatcher.h
  input_callback_dispatcher.cpp
  input_focus_runtime.h
  input_focus_runtime.cpp
  input_runtime_setup.h
  input_runtime_setup.cpp
  lock_screen_runtime.h
  lock_screen_runtime.cpp
  status_bar_runtime.h
  status_bar_runtime.cpp
  page_action_result.h
  overlay_runtime.h
  overlay_runtime.cpp
  page_input_runtime.h
  page_input_runtime.cpp
  shared_page_interactions.h
  # Per-page runtime families. Each feature page has a {runtime, coordinator,
  # interactions} trio (settings/wifi/time predate the coordinator split and
  # keep their state in the runtime):
  #   dashboard_page_*  onboarding_page_*  vibe_check_page_*  summarize_page_*
  #   notes_page_*  todos_page_*  follow_up_page_*  details_page_*
  #   settings_page_{runtime,coordinator,interactions}  wifi_page_*  time_page_*
  settings_page_interactions.h
  settings_page_interactions.cpp
  wifi_page_interactions.h
  wifi_page_interactions.cpp
  time_page_interactions.h
  time_page_interactions.cpp
  page_interaction_runtime.h
  page_interaction_runtime.cpp
  timeline_format.h                # shared timeline date/time formatters ("Today" logic)
  timeline_format.cpp
  ui_refresh_runtime.h
  ui_refresh_runtime.cpp
  app_interaction_result.h
  app_interaction_target.h
components/
  audio_hal/
  axp2101/
  board/
  button_service/
  design_tokens/
  device_sleep_service/
  display_service/
  epaper_panel/
  epaper_ui/
  feedback_service/
  gemini_service/
  i2c_device/
  imu_service/
  page_navigation/
  pcf85063/
  playback_service/
  power_service/
  project_assets/
  qmi8658/
  recording_archive_service/
  recording_service/
  recording_session_service/
  sd_card/
  storage_service/
  summary_service/
  system_sound_service/
  task_config/
  timezone_service/
  transcription_service/
  wifi_service/
partitions.csv
sdkconfig
sdkconfig.defaults
docs/
  app-architecture.md
  asset-generation.md
  auto-sleep.md
  gemini-service.md
  waveshare-epaper-hardware-spec.md
scripts/
  generate_epaper_assets_common.py
  generate_epaper_footer_icons.py
  generate_epaper_fonts.py
  generate_epaper_icons.py
  generate_epaper_logos.py
  generate_epaper_project_assets.py
```

## Component Boundaries

### `components/project_assets`

This component owns embedded assets that are compiled into the firmware image.
The source PNG/TTF files and generator scripts live outside the component; this
component exposes only the generated C++ data and a small app-facing lookup API.

Current scope:

- packed monochrome image metadata through `asset_types.h`
- manifest-driven asset generation through `assets/epaper_assets.json`
- generated e-paper logo assets for the ALXV Labs and Folloup logos
- generated e-paper icon assets for all PNG files currently in `assets/icons/`
- empty generated footer-icon scaffolding ready for future manifest entries
- `project_assets::GetLogo(...)`, `GetIcon(...)`, and `GetFooterIcon(...)`
  lookup helpers by generated enum IDs

Keep generated files reproducible from `assets/` and `scripts/`. Do not hand
edit generated asset source files. See `docs/asset-generation.md`.

### `components/design_tokens`

This header-only component owns shared product UI constants. It is intentionally
named `design_tokens`, not e-paper design tokens, because the values describe
the Folloup UI language rather than the SSD1677 display driver.

Current scope:

- spacing scale
- a canonical four-step e-paper grayscale ramp (`gray1` through `gray4`) plus
  semantic grayscale color roles
- typography roles, sizes, and weights
- common component sizing constants used by the e-paper UI being ported

Use this component as the first dependency when porting small pieces from the
old `epaper_lib`. Keep UI tokens independent from display hardware and
framebuffer mechanics.

### `components/epaper_ui`

This component owns reusable e-paper UI primitives that render into the app's
portrait framebuffer. It depends on `design_tokens` for visual constants and
`project_assets` for embedded icons/logos, but it does not depend on app
services or startup/runtime policy.

Current scope:

- generated Inter bitmap fonts used by e-paper UI typography roles
- a small role-aware bitmap font renderer
- the app status-bar state contract and renderer
- a reusable lock-screen renderer plus its dedicated state contract

App-owned runtime helpers in `main/` may compose service state into these UI
contracts, but the drawing primitives themselves should stay reusable and
service-agnostic.

### UI Layering

The e-paper UI stack is now intentionally split across three layers:

- `design_tokens` owns product-wide spacing, grayscale, typography, and
  component metrics.
- `epaper_ui` owns reusable presentation primitives such as bitmap fonts,
  role-aware text rendering, the status bar renderer, the lock-screen
  renderer, and future view widgets ported from `followup`.
- app-owned runtime helpers in `main/` compose service state into UI contracts.
  Today that includes `status_bar_runtime`, which translates
  `power_service`, `wifi_service`, `timezone_service`, and sleep/shutdown state
  into a neutral `epaper_ui::StatusBarState`, `footer_runtime`, which projects
  footer layout plus shared-focus projection into
  `epaper_ui::GlobalFooterState`, `overlay_runtime`, which owns retained
  modal/toast UI contracts, and `lock_screen_runtime`, which composes time plus
  status indicators into `epaper_ui::LockScreenState` and lock-screen
  visibility.

`display_service` remains the owner of the physical panel, framebuffer, refresh
mode decisions, and sleep/wake transitions. It may consume `epaper_ui`
renderers, but it should not become the home for product state composition or a
grab bag of reusable widgets.

### Screens and overlays

Screens are mutually-exclusive full-screen underlays selected by `ScreenId`.
Overlays composite on top of the active screen and (except the toast) capture
input while visible.

Screens (`ScreenId`):

- `kHome` — the dashboard: a focusable menu that opens the feature pages.
- `kOnboarding` — a first-boot carousel (Close / Prev / Next). Shown once, gated
  by NVS `app_state`/`onboarded`; re-launchable from Settings → "Manual".
- `kVibeCheck`, `kSummarize` — AI idea / summary cards.
- `kNotes`, `kTodos`, `kFollowUp` — recording timelines (two-level: date-group
  chips → an entered, scrollable item list) built on the `timeline_list`
  primitive and `timeline_format` (the "Today"/absolute-date labels).
- `kDetails` — a single recording's details with an entered transcript scroll
  container.
- `kSettings` (WiFi/AP toggles, Format SD, "Manual" onboarding), `kWifi`,
  `kTime`, `kLockScreen`.

Overlays (`overlay_runtime` + drawn in `display_service::DrawCurrentOverlays`,
z-order keyboard → toast → select modal → card modal → sticky note):

- **card modal** — shutdown/storage confirmations (replaces the old
  "shutdown modal").
- **select modal** — single-choice pickers (e.g. tag / timezone).
- **keyboard** — on-screen text entry.
- **toast** — transient status, optionally closable.
- **sticky note** — a full-page overlay opened by the footer Sticky button that
  flips through the follow-up notes (Prev/Next wrap, Close), with a Details-style
  scroll container for each transcript.

### Overlay refresh suppression

All page, status-bar, footer, and overlay repaints flow through
`ui_refresh_runtime`, a keyed latest-wins worker. Each caller schedules an
*apply callback* (which pushes fresh state into `display_service`) plus a
refresh request, keyed by a `SurfaceKey` (`kOverlay`, `kLockScreen`,
`kStatusBar`, `kFooter`, and one key per page: `kSettingsPage`, `kWifiPage`,
`kTimePage`, `kDashboardPage`, `kVibeCheckPage`, `kSummarizePage`, `kNotesPage`,
`kTodosPage`, `kFollowUpPage`, `kDetailsPage`, `kOnboardingPage`). The worker
coalesces pending work per surface and issues at most one screen (underlay)
refresh and one overlay refresh per drain.

**The overlay rule:** while an overlay owns the screen — that is, while
`overlay_runtime::IsInputCaptured()` is true (keyboard, select modal, card modal,
the sticky-note overlay, a shutdown request in progress, or a closable toast) —
the worker *suppresses underlay refreshes*
(page/status/footer). The apply callbacks still run, so the stored state stays
current; only the panel rebuild *beneath* the open overlay is skipped. Overlay
refreshes (the overlay repainting itself) always proceed. When the overlay
closes, the next underlay refresh resumes and the screen repaints with the
latest state (the overlay-dismiss path already requests that refresh).

This is a single global policy enforced in one place (`UiRefreshTask`), so it
applies uniformly to every screen rather than being re-implemented per event
handler. It exists because rebuilding a page underneath an open keyboard or
modal — for example on every clock tick while editing the time page — made
overlays feel laggy and, during rapid overlay navigation, could keep the
display task busy long enough to starve the idle task and trip the task
watchdog. Background events (clock ticks, Wi-Fi/scan updates, battery changes)
therefore keep their contracts in sync without forcing a visible underlay
rebuild while the user is busy inside an overlay. Auto-dismiss toasts do not
capture input, so they never suppress underlay refreshes.

Invariant: every `SurfaceKey` must map to a distinct slot in `ui_refresh_runtime`
(`SurfaceIndex` plus `kSurfaceCount`). A missing `SurfaceIndex` case silently
aliases that surface onto slot `0` (`kOverlay`), letting page refreshes clobber
the keyboard/modal's pending overlay refresh — keep them in sync when adding a
screen.

### `main`

`main/` owns product composition for this firmware. It is not a reusable
component. Keep it focused on startup ordering and app-level orchestration.

`main/main.cpp` is intentionally tiny: it is only the ESP-IDF `app_main()` entry
point and delegates to `app_shell::Run()`.

`main/app_shell.cpp` is an orchestration layer only. It may decide startup order,
connect app-level policies, and choose whether an optional service failure is
fatal, but it should not contain hardware driver logic, protocol logic, button
debouncing, battery math, display drawing, networking workflows, or long-running
feature loops. Put those behaviors in services/components and call them from the
app shell.

`main/status_bar_runtime.cpp` is an example of the intended app-runtime helper
pattern. It is not a reusable component and does not own hardware or rendering.
Its job is to compose product state into UI-facing data contracts that
`display_service` can render through `epaper_ui`.

The current app-runtime helpers under `main/` are:

- `status_bar_runtime`: compose Wi-Fi, Gemini, battery, sleep, and shutdown
  state into `epaper_ui::StatusBarState`
- `footer_runtime`: project footer layout and shared page focus into
  `epaper_ui::GlobalFooterState` (Settings/WiFi/Time/Folder/Sticky/Home + Mic)
- `overlay_runtime`: own retained overlay state (card modal, select modal,
  keyboard, toast, and the full-page sticky-note overlay), hit testing, and
  overlay presentation hooks
- one runtime family per feature page — `{dashboard, onboarding, vibe_check,
  summarize, notes, todos, follow_up, details}_page_{runtime, coordinator,
  interactions}`, plus `settings/wifi/time` (runtime + interactions) — composing
  page state and translating focus into neutral page outcomes + follow-on intents
- `timeline_format`: shared date/time formatters for the Notes/Todos/Follow-up
  timelines and the sticky-note overlay (the "Today"-vs-absolute-date logic)
- `input_runtime_setup`: own app-facing button/touch binding setup plus the
  shared inputs-enabled gate before events enter app routing
- `input_focus_runtime`: own overlay-first button routing for roving focus
  movement plus app-wide touch contact precedence
- `page_input_runtime`: own active page input routing for the current
  page-owned screens, including focus movement, page-local button activation,
  footer projection hooks, touch-provider registration, and applying neutral
  page interaction results into app-facing behavior
- `settings_page_interactions` / `wifi_page_interactions` /
  `time_page_interactions`: own page-local focus and activate semantics for the
  current page-owned screens so the shared page-input layer applies
  intent/results instead of open-coding page behavior inline inside each runtime
- `page_interaction_runtime`: own the registration contract future page
  runtimes/coordinators use to plug page targets into the shared touch
  interaction path
- `lock_screen_runtime`: own lock-screen visibility and clock-state composition
- `ui_refresh_runtime`: own the keyed latest-wins UI presentation worker, and
  enforce the global overlay refresh rule (see "Overlay refresh suppression")

The current early startup sequence is:

- Detects whether the running image is `ESP_OTA_IMG_PENDING_VERIFY`.
- Marks the image valid with `esp_ota_mark_app_valid_cancel_rollback()`.
- Brings up the AXP2101 rails before OTA validation.
- Initializes `power_service`.
- Logs one power/battery diagnostic snapshot.
- Initializes `feedback_service` and requests the startup feedback.
- Initializes `storage_service` and logs one MicroSD diagnostic snapshot.
  On this board, when a card is present, storage must initialize before the
  shared-bus display path so the card enters SPI mode first and remains mounted.
- Initializes `display_service` and clears the e-paper panel to a blank screen.
- Initializes `ui_refresh_runtime`, which owns the shared latest-wins UI
  presentation worker.
- Initializes `overlay_runtime`, which owns global modal/toast overlay state,
  shutdown-confirm focus, and overlay presentation hooks.
- Initializes `imu_service` and logs three direct IMU samples for bring-up.
- Initializes `power_key_runtime`, attaching the PMIC power-key handler that
  routes a short press to the lock screen and a long press to the shutdown
  confirmation.
- Starts the auto-sleep runtime, which wires `device_sleep_service`, polls IMU
  samples for inactivity, owns the auto-sleep worker task, and handles display
  sleep/light sleep actions.
- Initializes `timezone_service`, which loads timezone/time-sync state from
  NVS, applies the configured timezone, and seeds system time from the PCF85063
  RTC when available.
- Initializes `wifi_service`, which loads saved Wi-Fi credentials or built-in
  sdkconfig credentials, starts station mode when credentials exist, or starts
  AP setup mode when no credentials are available.
- Initializes `recording_service` and logs recording status.
- Initializes `recording_session_service`, which owns the press/hold recording
  flow, tag selection, and transcription/save orchestration.
- Initializes `footer_runtime`, which seeds the footer layout (Settings, WiFi,
  Time, Folder, Sticky, Home buttons — in that left-to-right order — plus the Mic
  status) and the footer focus projection model. The Sticky button (left of Home)
  opens the follow-up sticky-note overlay.
- Initializes `button_service`.
- Subscribes to button and touch events, forwards user activity into
  auto-sleep, forwards interaction feedback into `feedback_service`, and handles
  button-driven lock-screen, refresh, and shutdown intents.
- Subscribes to Wi-Fi events and forwards connection state into
  `timezone_service` so network time sync starts after station connectivity is
  available.
- Runs a small shutdown task so button callbacks can request shutdown without
  directly executing the PMIC power-off sequence.
- Seeds the status bar and footer state (no refresh), then renders the first
  screen with a single **full** refresh as the first thing the panel paints, and
  finally sets `s_startup_complete`. On first boot (NVS `app_state`/`onboarded`
  unset) that first screen is the onboarding page (`ShowOnboardingScreen(kFull)`);
  otherwise it is the dashboard home (`ShowHomeScreen(kFull)`).

**Boot refresh policy — no partial refresh before the initial full paint.** The
first thing the panel paints on boot is that single **full** refresh of the first
screen (onboarding or home, the last step above). No partial
refresh is allowed before it. Services initialize *before* that paint and several
publish events during boot (the RTC time intent, Wi-Fi connection state, the
recording-archive snapshot, storage mount); their handlers update UI state but
must **not** request a refresh yet, because the upcoming full paint already
redraws every surface — an earlier partial is redundant work, and a partial on a
freshly-powered panel that has no full-flush baseline ghosts. The rule is
enforced by gating every handler's refresh request on `s_startup_complete`,
folded into the `ScreenActiveForRefresh(screen)` predicate
(`s_startup_complete && GetCurrentScreen() == screen`); the status bar and footer
paths check `s_startup_complete` directly. Anything new that repaints in response
to a service event must route through the same gate so it stays silent until the
initial full paint lands. After boot the predicate reduces to "is this screen
active," so live updates partial-refresh normally.

Current app-level button interactions are:

- `UP` / `DOWN` move roving focus (or scroll an entered control) one step per
  press, with wraparound, on the active screen. Navigation is driven on
  press-down; a plain `UP` / `DOWN` single click (the release event) is inert.
- `BOOT` and the rocker's middle key (`FN`) both single-click to activate /
  submit the focused item on the active screen (footer target, page control, or
  modal action). `button_service::IsPrimaryButton` is what makes the two
  equivalent.
- Pressing and **holding** `DOWN` (a long-press) is the app-wide "exit an entered
  control" gesture, handled per screen: it backs out of a control the user has
  stepped into -- e.g. the Vibe Check card, an entered scroll container /
  timeline item list on the Summarize / Notes / Todos / Follow-up pages, the WiFi
  network list, or the sticky-note transcript scroll. It is a no-op at the app
  level. (This replaced the former `DOWN` double-click exit.)
- A short press of the `PWR` key toggles the lock screen; a ~1s hold opens the
  shutdown confirmation modal. `PWR` is not a GPIO button: both arrive as AXP2101
  interrupts, decoded by `main/power_key_runtime`. A sustained 6s hold bypasses
  firmware entirely and the PMIC cuts the rails.
- The rocker middle key has no double-click or long-press action. Lock and
  shutdown moved to `PWR`; recording is exclusive to `BOOT`.
- Pressing and holding `BOOT` arms then starts the recording-session flow;
  releasing stops it. See [Recording Flow](#recording-flow) for what happens
  between the release and the tag menu.
- while the select modal is visible, `UP` and `DOWN` press down plus gated
  hold-repeat move roving focus with wraparound, a primary-button click submits the focused
  item, and touch focuses the touched item on contact before submitting on
  release.
- while the shutdown modal is visible, `UP` and `DOWN` press down plus gated
  hold-repeat move roving focus with wraparound, a primary-button click activates the
  focused action, and touch focuses `Cancel` or `Shut down` on contact before
  activating on release.
- when no overlay captures input, footer targets participate in the same touch
  model: touch-down focuses the footer item immediately and touch-up activates
  the armed footer target. On page-owned screens, that touch-down focus is
  translated straight into page-local focus truth before footer projection is
  repainted.

Shutdown still runs through the deferred AppShell shutdown task so the
PMIC power-off sequence does not execute inside the button callback. The
shutdown chord now routes through the app-owned input/overlay path first, and
only a confirmed modal action notifies the task. The task waits briefly before
calling `power_service::RequestShutdown()` so the analog button/Q2 bootstrap
path has time to stop feeding `PWR_EN`.

Current input precedence and focus ownership are:

- overlay roving/hit focus first: card modal, select modal, keyboard, and the
  full-page sticky-note overlay, plus the closable toast
- footer targets when no overlay captures input
- registered page targets after footer under the shared page-touch contract

Current focus-surface inventory is:

- `Home`: the dashboard page (focusable menu) plus the footer
- `Onboarding`: the carousel page (Close / Prev / Next controls); no footer
- `VibeCheck`, `Summarize`: shared page-focus path
- `Notes`, `Todos`, `FollowUp`: shared page-focus path with a two-level timeline
  (date-group chips → entered item list)
- `Details`: shared page-focus path with an entered transcript scroll container
- `Settings`: shared page-focus path (incl. the "Manual" onboarding button)
- `WiFi`: shared page-focus path, with page-owned list sub-focus for networks
- `Time`: shared page-focus path, with overlay editors (timezone select modal,
  numeric keyboard per field)
- `Lock screen`: no focusable page or footer surface today
- card modal / select modal / keyboard / toast / sticky-note overlays: overlay path

Ownership is intentionally split as:

- `components/page_navigation/roving_focus`: reusable wraparound index
  primitive with no modal, display, or app-shell ownership baked in
- `components/page_navigation/navigation_input_controller.*`: shared press
  generation and hold-repeat gating for navigation buttons
- `main/input_callback_dispatcher.*`: dedicated latest-wins input callback task
  for app-owned button routing
- `main/input_runtime_setup.*`: app-owned raw button/touch binding setup plus a
  shared inputs-enabled gate before app routing begins
- `main/button_input_runtime.*`: app-wide hardware-button dispatch policy that
  converts raw button events into shared navigation press/hold behavior before
  they reach page or overlay code
- `main/input_focus_runtime.cpp`: app-owned focus routing, touch contact state,
  and app-wide precedence for overlay, footer, and page targets
- `main/page_input_runtime.*`: active page input routing for current page-owned
  screens so `app_shell` and `input_focus_runtime` do not hard-code page
  behavior directly, and so neutral page interaction results are applied in
  one place instead of inside individual page runtimes
- `main/settings_page_interactions.*`, `main/wifi_page_interactions.*`, and
  `main/time_page_interactions.*`: focused interaction helpers that translate
  current page focus into neutral page outcomes plus follow-on intents, while
  leaving service effects and orchestration callbacks outside the coordinator
- `main/page_interaction_runtime.cpp`: registration point for future page
  runtimes/coordinators to provide `resolve -> focus -> activate` touch hooks
- `main/overlay_runtime.cpp`: retained overlay state, focus-sync, submit, and
  dismiss behavior for the card modal, select modal, keyboard, toast, and
  sticky-note overlays
- `main/footer_runtime.cpp`: presentation-only projection of footer layout and
  shared page focus into the e-paper footer contract, plus footer
  touch resolve/focus/activate hooks for footer-owned surfaces such as `Home`
- `main/app_shell.cpp`: orchestration only; wires button/touch events into the
  focused runtime helpers and composes higher-level product policy

The current shared page-touch contract for current and future page-owned
screens is:

- `resolve_touch_target(x, y, target)`: identify whether a page-owned
  interactive target was touched
- `focus_touch_target(target)`: update page-owned focus truth immediately
- `activate_touch_target(target)`: perform page-owned activation on touch
  release

This contract is implemented by every page-owned screen: `Dashboard` (home),
`Onboarding`, `VibeCheck`, `Summarize`, `Notes`, `Todos`, `FollowUp`, `Details`,
`Settings`, `WiFi`, and `Time`. Dispatch for the active screen is centralized in
`main/page_input_runtime.cpp` (`resolve/focus/activate` and button handling per
`ScreenId`).

Future pages should keep page-local selected indexes as render projections of
page-owned focus truth rather than inventing separate touch-only selection
state. Composite page controls should plug into this same contract instead of
adding a second touch interaction path.

Page-owned screens also own footer focus truth whenever their footer buttons are
part of the same navigation model. Touch-down on `Settings` or `WiFi` footer
targets is translated into the page coordinator's focus index first, and the
footer is then repainted as a projection of that page-local state. The footer
runtime keeps standalone focus ownership only on footer-owned surfaces such as
`Home`.

Shared button-navigation rules are:

- navigation timing is not page-owned
- navigation `press down` and gated hold-repeat behavior route through the
  shared input runtime first
- shared hold-repeat uses an explicit first-repeat gate before interval-based
  repeats so the timing stays stable even if raw repeat callbacks jitter
- on the active WiFi network list, hold-repeat uses page jumps sized to the
  currently visible row capacity, while press-down still advances by one row
- stale queued navigation callbacks should be superseded by the newest callback
  for the same button lane
- page modules own `MoveFocus(...)`, activate semantics, and retained page-state
  construction only

The current app-wide touch lifecycle is:

- touch-down resolves the highest-precedence target and focuses it immediately
- touch-move may retarget focus while the contact stays active
- touch-up activates only the armed target from that contact
- touch-up with no armed target cancels activation without inventing a second
  selection state

The current app-wide interaction feedback lifecycle is:

- page, footer, overlay, and input-focus helpers may decide that an interaction
  should produce product feedback, but they should emit only neutral
  app-owned feedback cues
- shared interaction contracts such as `main/app_interaction_result.h` should
  use app-owned cue enums rather than depending on `feedback_service` or
  `system_sound_service` types directly
- retained overlay state may queue a pending neutral feedback cue when modal or
  toast presentation changes, but it should not play sound feedback directly
- `main/app_shell.cpp` is the single place that maps neutral app-owned feedback
  cues onto `feedback_service` events and requests actual playback

This keeps interaction ownership local while preventing feedback policy from
leaking into reusable runtime helpers or shared interaction contracts.

### Time configuration page

The time configuration page is the first screen ported end-to-end through the
full page pattern, and is the reference example for adding a page. It is reached
from the global footer `Time` button and is composed across five layers:

- **View renderer** (`components/epaper_ui/time_page.*`): a stateless
  `DrawTimePage` plus `TimePageState`, bounds, and hit-test. Like the other page
  renderers it lives in `epaper_ui` (not `main/`) because `display_service` — a
  component — draws it and cannot depend on `main`. It composes the page's
  primitives: `select_input` (timezone), `time_input` (hour / minute / month /
  day / year), and `button` (AM-PM and Sync & Save). `text_input` is the shared
  field primitive those build on, and which `password_input` now wraps.
- **Coordinator** (`main/time_page_coordinator.*`): owns the editable field
  values, the navigation model plus roving focus, loads state from
  `timezone_service`, and builds `TimePageState` and the save patch. A
  `user_edited_` guard stops background clock events from clobbering in-progress
  edits; `MarkSaved()` clears it after a save so a later sync reloads the page.
- **Interactions** (`main/time_page_interactions.*`): map the focused control to
  a neutral activate intent (open timezone modal, edit a numeric field, toggle
  AM/PM, save, or footer navigation) with no side effects.
- **Runtime** (`main/time_page_runtime.*`): the mutex-guarded orchestrator —
  focus movement, touch resolve/focus/activate, footer projection, the overlay
  editors, and the save flow. State is pushed to `display_service` and refreshes
  are scheduled through `ui_refresh_runtime` (`SurfaceKey::kTimePage`).
- **Integration**: `display_service` gains `ScreenId::kTime`, `SetTimePageState`,
  and an `ApplyTime` / `DrawTimeUnderlay` path; `page_navigation` gains the
  `kTime` scope, the control roles, and `BuildTimePageNavigationModel`;
  `page_input_runtime` routes the screen; and `app_shell` exposes
  `ShowTimeScreen` plus the footer `Time` entry.

Field editing happens in overlays, so it inherits the overlay refresh rule
above:

- The timezone control opens a scrollable `select_modal` over
  `timezone_service::ListTimezones()`; the chosen index is committed through the
  runtime's select-modal submit hook.
- Each numeric field opens the keyboard in its `kNumbers` layout — a standalone
  dial-pad (`1`-`9`, then `Bksp | 0 | Done`); the typed value is committed on
  submit.

The save / sync flow:

- `BuildSettingsPatch` converts the fields (12h + AM/PM to 24h, `YYYY-MM-DD` and
  `HH:MM`) and `Save()` calls `timezone_service::ApplySettingsPatch`, then shows
  a result toast. The patch's internal `Notify` drives `HandleTimezoneEvent`,
  which already re-syncs the page, so `Save()` does not also run a redundant
  full sync.
- `ApplySettingsPatch` never runs the blocking SNTP path on the caller's task.
  When the network is up it queues the NTP sync on the dedicated `timezone_sync`
  worker (`QueueSync`); the result returns asynchronously via the SNTP callback
  -> `Notify` -> event. Running SNTP inline overflowed the small touch-task
  stack.
- The clock also re-syncs on every Wi-Fi reconnect: `SetNetworkConnected` queues
  a sync on the disconnected -> connected transition whenever the clock is
  enabled and a timezone is set (default Eastern). The transition guard keeps
  repeated "connected" events from spamming NTP.

The `location` field in the underlying `timezone_service` settings is
intentionally not surfaced on this page. It is metadata only (kept in NVS and
the web portal) and has no effect on timekeeping, which is driven solely by the
timezone selection plus NTP.

## Recording Flow

`recording_session_service` owns the whole press-and-hold take, from the first
cue to the tag menu. The phase machine is:

```text
kIdle -> kArmed -> kStartCue -> kRecording -> kStopCue -> kPlayingBack
      -> kAwaitingTagSelection -> kSaving -> kTranscribing -> kComplete
```

- **kArmed** — `BOOT` press-down arms the recorder.
- **kStartCue** — the hold threshold fires, capture starts, and the start cue
  (`SoundCue::kSpeaking`) plays. Capture deliberately starts *before* the cue:
  waiting for the cue to finish would swallow the speaker's first word, so the
  cue overlaps the opening moments of the take.
- **kRecording** — entered when the start cue completes. If `BOOT` is released
  while still in `kStartCue`, the finish is deferred rather than dropped
  (`s_finish_pending_after_start_cue`); without that, a hold barely longer than
  the cue would never stop.
- **kStopCue** — release finishes capture and plays the stop cue
  (`SoundCue::kInterrupt`). Playback waits for the cue to complete rather than
  overlapping it, since both share the one codec output.
- **kPlayingBack** — the take is replayed to the user from the PSRAM chunks, via
  `playback_service::PlayClip`, on a short-lived worker task. Playback blocks for
  the length of the clip, so it cannot run on the cue-callback task.
- **kAwaitingTagSelection** — the tag menu opens. **The clip is still only in
  PSRAM at this point.** That is the reason playback comes first: the user hears
  the take, and the menu's `Discard` option throws away a bad one without it ever
  reaching the SD card. Saving happens in `kSaving`, after a tag is chosen.

Every route out of `kStopCue` converges on `AdvanceToTagSelection` — playback
finished, playback could not start, or the stop cue itself failed — so a missing
or broken cue degrades to "no replay" rather than stranding the session.

Cue callbacks carry a token that is bumped on every queued cue and on
`ResetToIdleLocked`, so a result arriving after a cancel is dropped instead of
driving a stale transition.

Auto-sleep is blocked while `playback_service::IsPlaying()`, since neither the
replay nor the Details page's play action touches recording state and the
inactivity timer would otherwise keep counting through the clip.

## Task Mapping

App-owned FreeRTOS tasks use the shared mapping in
`components/task_config/include/followup_task_config.h`. The app is optimized
around a simple split:

- CPU0 is the system/network side. ESP-IDF already runs the main task,
  `esp_timer`, and Wi-Fi driver work there in the current `sdkconfig`, so app
  Wi-Fi/time coordination stays close to that side.
- CPU1 is the product hardware/UI side. Touch, audio capture, storage work,
  sound feedback, and sleep-driven display transitions are kept away from CPU0
  as the app scales.

On single-core builds, the shared task config maps the app core back to CPU0.

| Task | Owner | Priority | Core | Responsibility |
| --- | --- | ---: | --- | --- |
| `record_capture` | `recording_service` | 5 | CPU1 | Timing-sensitive microphone capture, pre-roll, and clip buffering. |
| `axp2101_irq` | `axp2101` | 2 | CPU1 | PMIC interrupt servicing, including power-key short/long press. |
| `app_sleep` | `device_sleep_runtime` | 4 | CPU1 | Display sleep, light-sleep entry/exit, and wake recovery actions. |
| `app_shutdown` | `app_shell` | 4 | CPU1 | Deferred PMIC power-off after the shutdown modal is confirmed. |
| `sleep_motion` | `device_sleep_runtime` | 3 | CPU1 | 200 ms IMU polling and motion/stillness classification. |
| `wifi_transition` | `wifi_service` | 3 | CPU0 | Wi-Fi station/AP/stop/disconnect transitions. |
| `wifi_callbacks` | `wifi_service` | 3 | CPU0 | App-facing Wi-Fi event delivery outside ESP event callbacks. |
| `storage_service` | `storage_service` | 2 | CPU1 | Long-running SD operations such as format. |
| `timezone_sync` | `timezone_service` | 2 | CPU0 | SNTP sync, system-time update, and RTC writeback. |
| `clip_playback` | `recording_session_service` | 2 | CPU1 | Short-lived worker that replays a just-recorded clip. |

The mapping intentionally keeps long-running SD work below input and audio
capture. Future tasks should be added to `task_config` first, with a short
ownership rationale, rather than using local priority/core literals.

Driver-specific wiring should stay out of `main/`; app startup should call
service-level APIs instead. Add product-specific sequencing in `app_shell`, not
inside reusable components.

Wi-Fi and time services follow the same boundary:

- `wifi_service` owns `esp_netif`, the default ESP event-loop registration,
  `esp_wifi` mode changes, station/AP configuration, NVS credential storage,
  network scans, and the HTTP backend server used during AP setup.
- `timezone_service` owns timezone catalog/aliases, persisted timezone settings,
  SNTP setup, system-time updates, PCF85063 RTC read/write through
  `power_service`, and backend HTTP routes for time settings.
- `app_shell` wires the two services together by forwarding Wi-Fi connectivity
  events into `timezone_service::SetNetworkConnected(...)`.

Runtime-persisted settings live in service-owned NVS namespaces:

- `wifi`: `ssid`, `password`
- `timezone`: `enabled`, `tz_name`, `location`, `time_src`, `ntp_sync`,
  `ntp_epoch`

The build-time Wi-Fi/time defaults live under `Folloup Settings`:

- `CONFIG_FOLLOWUP_WIFI_AP_PREFIX`
- `CONFIG_FOLLOWUP_WIFI_STA_SSID`
- `CONFIG_FOLLOWUP_WIFI_STA_PASSWORD`
- `CONFIG_FOLLOWUP_WIFI_START_IN_AP_MODE`
- `CONFIG_FOLLOWUP_TIME_SYNC_DEFAULT_ENABLED`
- `CONFIG_FOLLOWUP_DEFAULT_TIMEZONE_NAME`

Saved NVS Wi-Fi credentials take precedence over built-in sdkconfig
credentials. If neither exists, or if `CONFIG_FOLLOWUP_WIFI_START_IN_AP_MODE`
is enabled, `wifi_service` enters open AP setup mode and serves backend routes
at the SoftAP URL, normally `http://192.168.4.1`. The current backend
intentionally exposes JSON/form endpoints only; it does not embed the old
portal UI and does not add DNS captive-portal redirection.

Current Wi-Fi backend routes:

- `GET /`
- `GET /api/status`
- `GET /api/scan`
- `POST /api/configure`
- `POST /api/disconnect`

Current time backend routes registered on the same HTTP server:

- `GET /api/settings/time`
- `PATCH /api/settings/time`
- `GET /api/runtime/time`
- `GET /api/timezone/list`

Auto-sleep is split across a policy component and a product runtime helper:
`device_sleep_service` owns sleep state, timers, timeout validation, blocker
state, and transition events, but it does not touch display, GPIO, or ESP sleep
hardware. `main/device_sleep_runtime.cpp` owns product-specific auto-sleep
runtime behavior: IMU inactivity polling, the event worker task, display sleep
commands, ESP light-sleep entry, wake handling, and app-level blocker
aggregation. `app_shell` should only provide settings, provide app-owned
signals such as shutdown-pending state, start the runtime, and forward user
activity. See `docs/auto-sleep.md` for the stable feature behavior and the
deferred FIFO/shared-interrupt plan.

Current auto-sleep behavior:

- `main/device_sleep_runtime.cpp` polls `imu_service::ReadSample(...)` every
  `200 ms` and converts acceleration deltas from `g` to `mg`.
- Motion is detected when the axis-delta sum is at least `60 mg` or the largest
  axis delta is at least `25 mg`.
- Stillness is detected only after a continuous `2 s` window where the
  axis-delta sum is at most `20 mg` and the largest axis delta is at most
  `8 mg`.
- Display sleep refreshes the e-paper panel to a blank screen and then puts the
  panel to sleep.
- ESP32-S3 light sleep waits for `ACTION` / `GPIO0` to be released, then arms
  `ACTION` and the PMIC IRQ / `GPIO38` as active-low `gpio_wakeup_enable`
  sources. It suspends button polling (light sleep's clock jump would otherwise
  replay every missed tick on wake and destroy click classification), arms
  wake-only `ACTION` event suppression so the wake press cannot start a
  recording, refreshes the panel to a blank screen, puts the panel to sleep, and
  enters `esp_light_sleep_start()`. There is no power latch to preserve: the
  AXP2101 holds the rails across sleep.
- The wake-causing power-button events are consumed as wake-only after light
  sleep, so they do not trigger normal power-button behavior or leave
  `shutdown_pending` set as an auto-sleep blocker.
- After light-sleep wake, the display is restored to a blank screen with a
  forced full refresh.
- Inactivity is blocked while recording is active, armed, saving, or exporting;
  while a clip is playing back;
  while shutdown is pending; while an e-paper refresh is active; during
  app-declared storage write activity; while AP setup mode is active; and while
  SNTP time sync is in progress.
- The current bench defaults are `10 s` for display sleep and `30 s` for light
  sleep. Production defaults should be raised later when product behavior is no
  longer being tuned on the bench.

SD-card formatting remains exposed through `storage_service`, but no demo
button path currently invokes it. A future app UI should call the storage API
through its own action/controller layer.

### `components/axp2101`

This is the AXP2101 PMIC driver. On this board the PMIC is not optional: it feeds
every rail, so a missing or unresponsive chip is unrecoverable by design and the
constructor aborts.

Current scope:

- own the DC1 / ALDO1-3 rails, the single-cell charger profile, and the
  system power-down voltage
- expose battery level, voltage, temperature, charge state, and VBUS presence
- own the power-key timings: press-to-power-on, the IRQ level time that splits a
  short press from a long one, and the hardware press-to-power-off hold
- run a dedicated IRQ task that reads and clears the status register, then hands
  a decoded `InterruptEvent` to a registered callback in task context

The driver stays app-agnostic. What a power-key press *means* belongs to
`main/power_key_runtime`, not here.

### `components/pcf85063`

This is the PCF85063 RTC driver. It shares the sensor I2C bus with the PMIC and
IMU.

Current scope:

- read and write wall-clock time
- back the timezone service's RTC writeback path
- keep timezone policy and SNTP scheduling out of the driver

### `components/board`

This component centralizes Waveshare-specific hardware access. It is the only
place that knows this board's pin mapping; generic drivers stay board-agnostic
and are composed here.

`waveshare_board_config.h` owns the pin map:

- ACTION / BOOT button: `GPIO_NUM_0` (also the light-sleep wake button)
- rocker up: `GPIO_NUM_4`
- rocker middle / FN: `GPIO_NUM_5`
- rocker down: `GPIO_NUM_6`
- e-paper (SSD1677) on a dedicated SPI3 bus: BUSY `GPIO_NUM_3`, DC `GPIO_NUM_9`,
  CS `GPIO_NUM_10`, SCK `GPIO_NUM_11`, MOSI `GPIO_NUM_12`, RST `GPIO_NUM_46`,
  no MISO
- MicroSD over the SDMMC controller, 4-bit: CLK `GPIO_NUM_16`, CMD `GPIO_NUM_17`,
  D0 `GPIO_NUM_15`, D1 `GPIO_NUM_7`, D2 `GPIO_NUM_8`, D3 `GPIO_NUM_18`
- shared sensor I2C: SDA `GPIO_NUM_41`, SCL `GPIO_NUM_42` — carries the AXP2101
  PMIC (`0x34`), the QMI8658 IMU, the PCF85063 RTC, the ES8311 codec control
  interface, and an SHTC3 that nothing drives
- PMIC interrupt: `GPIO_NUM_38`
- ES8311 audio over I2S0: MCLK `GPIO_NUM_13`, BCLK `GPIO_NUM_14`, WS
  `GPIO_NUM_47`, DIN `GPIO_NUM_21`, DOUT `GPIO_NUM_48`
- NS4150B power-amp enable: `GPIO_NUM_39`
- panel geometry: 800 x 480

There is no power latch on this board and no shared SPI bus to arbitrate: the
AXP2101 holds the rails, and the panel owns SPI3 outright. Both were significant
sources of Sticky-era complexity that simply do not apply here.

`waveshare_board.h/.cpp` owns:

- `waveshare_board::EnablePowerHold()` — brings up the AXP2101 rails, charger,
  and power-key behavior, and is the first thing `app_shell::Run()` calls
- `waveshare_board::GetPmic()` — the shared `Axp2101` instance
- `waveshare_board::GetAudioCodec()` — the shared `Es8311Codec` instance, created
  on first use with output (and therefore the PA) enabled for its lifetime
- `waveshare_board::EnsureSensorI2cBus(...)` — the one shared I2C master bus

Audio runs full duplex at a single 16 kHz clock for both capture and playback,
chosen to match the recording and Gemini pipeline so no resampling is needed
anywhere in the path. Output volume is set to `WAVESHARE_AUDIO_OUTPUT_VOLUME`
(full scale) before output is enabled — the NS4150B into a small MX1.25 speaker
has no headroom to give away, and the codec's own default is well below what is
audible in the hand.
### `components/power_service`

This component is the app-facing power layer. It composes the `board` helpers
with the AXP2101 PMIC and the PCF85063 RTC.

Current responsibilities:

- expose `power_service::EnablePowerHold()` so `main` can bring up the PMIC rails
  as the first application action
- expose `power_service::ReadStatus(...)`
- log one diagnostic snapshot through `power_service::LogDebugStatus()`

The current diagnostic snapshot includes:

- service initialization state
- battery level, voltage, and temperature from the PMIC fuel gauge
- charge state and full-charge detection
- VBUS presence and voltage
- PCF85063 control/status-2 bits for alarm/timer flags and interrupt enables

The AXP2101 owns rails, charging, and battery telemetry, so there are no charger
GPIOs to configure and no power-input ADC to sense -- both were Sticky-specific
and have no counterpart here.

`power_service::RequestShutdown()` is the app-facing shutdown entry point,
reached from the shutdown confirmation modal that a long `PWR` press opens. It
first clears and disables PCF85063 alarm/timer interrupt sources, then calls
`Axp2101::PowerOff()`, which cuts every rail. On battery the board goes dark
there and never returns; while VBUS is present the PMIC keeps the rail fed, so
the call can return with the board still powered -- the log says as much, and
unplugging USB completes the power-down.
### `components/button_service`

This C++ component owns app-facing button initialization and logging. It uses
Espressif's managed `espressif/button` component for the underlying debounce and
button-event state machine.

Current scope:

- `ACTION` / BOOT on `GPIO0`
- `UP` on `GPIO5`
- `DOWN` on `GPIO6`
- active-low GPIO buttons with internal pulls enabled by the managed component
- logs press down, press up, single click, double click, long press start, and
  long press up
- exposes a typed event callback API for app-level policy routing in
  `app_shell`

Current app-shell usage on top of those low-level events is:

- `UP` / `DOWN` press down: move roving focus (wraparound), one step per press.
  A plain `UP` / `DOWN` single click (the release) is inert.
- `BOOT` or rocker-middle `FN` single click: activate / submit the focused item
- hold `DOWN` (long-press): app-wide "exit an entered control" gesture, handled
  per screen (no-op at the app level; replaced the former `DOWN` double-click)
- short `PWR` press: toggle the lock screen (an AXP2101 interrupt, not a GPIO button)
- ~1s `PWR` hold: open the shutdown confirmation modal
- hold `BOOT`: arm/start/finish the recording-session flow
- select modal visible: `UP` and `DOWN` press down plus gated hold-repeat move
  shared roving focus, and a primary-button click submits
- shutdown modal visible: `UP` and `DOWN` press down plus gated hold-repeat
  move shared roving focus, and a primary-button click activates the focused
  action

The app shell does not own modal focus routing directly. It hands button events
to `main/input_focus_runtime.cpp`, which gives overlay focus traps first chance
to consume navigation movement, then defers submit/dismiss work to
`main/overlay_runtime.cpp`. `overlay_runtime` keeps the modal above both the
home screen and lock screen and only returns a `request_shutdown` intent after
explicit confirmation.

The auto-sleep runtime arms `ACTION` / `GPIO0` and the PMIC IRQ / `GPIO38` as
light-sleep GPIO wake sources. The managed button component still owns normal
awake-state debounce and event generation; light-sleep wake setup stays in
`main/device_sleep_runtime.cpp` so pre-sleep wake-only event suppression, button
poller suspend/resume, and immediate display recovery remain part of the auto-sleep
policy.

### `components/audio_hal`

This is the ES8311 codec layer. It replaces the Sticky's PDM-input-only path:
this board has both a microphone and a speaker (ES8311 plus an NS4150B power
amp), so the codec is full duplex.

Current scope:

- configure I2S0 for full-duplex 16 kHz mono 16-bit PCM, matching the recording
  and Gemini pipeline so nothing has to resample
- own the NS4150B power-amp enable pin alongside codec output enable
- expose `OutputData(...)` for playback and the capture side for recording
- expose output volume control

The board keeps codec output (and therefore the PA) enabled for the codec's
lifetime. Per-event PA toggling was rejected: cues and clip playback share the
output, and toggling clipped whichever stream started second.

### `components/system_sound_service`

This component owns the decoded sound-cue catalog and streams cues to the codec.

Current scope:

- decode and cache the built-in cue set
- play a cue, optionally with a completion callback reporting whether the cue
  completed, was debounced, superseded, interrupted, or failed
- serialize cue playback so two cues cannot interleave on the output

The completion callback is what lets `recording_session_service` sequence the
stop cue and the review playback without them overlapping.

### `components/feedback_service`

This C++ component owns app-facing haptic/audio feedback policy. It maps product
events onto sound cues without exposing codec details to
`app_shell`.

Current scope:

- initializes `system_sound_service` with the board's audio codec
- maps startup, lock, unlock, button click, button double-click,
  button long-press, shutdown, and error feedback onto sound cues
- keeps app-level feedback names separate from low-level cue names

`feedback_service` is intentionally an app-shell dependency, not a runtime-helper
dependency. App-owned helpers under `main/` such as `overlay_runtime`,
`input_focus_runtime`, `footer_runtime`, and future page runtimes should not
call `feedback_service` directly. They should emit neutral app-owned feedback
cues and let `app_shell` map those cues onto `feedback_service` events.

`app_shell` may request feedback events for button single-click, button
double-click, non-power long-press-start, lock, unlock, touch contact, modal
open, startup, shutdown, and other product-level interaction outcomes. It
should not know about LEDC timer numbers, PWM duty values, GPIO setup, or exact
sound-cue catalog composition.

### `components/sd_card`

This is the SDSPI/FATFS MicroSD wrapper ported from:

```text
/Users/tieuvong/Desktop/folloup/sticky_port/Device_Peripheral_Demo/components/sd_card
```

The component is mostly board-agnostic. It receives an `SdCardPins` struct and
mount point from its caller, then owns:

- SD power-enable GPIO configuration
- card-detect GPIO configuration
- SDSPI bus/device setup, unless the caller marks the SPI bus as externally
  owned
- FATFS mount/unmount at the requested mount point
- storage statistics
- directory listing
- small file read/write/append/truncate helpers

Do not make this component depend on `board`; pass pins in from the service or
board layer. On Sticky, `storage_service` asks the board layer to initialize the
shared SPI bus first and passes `external_spi_bus=true`, so the SD wrapper only
adds its SDSPI device to the existing bus.

### `components/storage_service`

This is the app-facing storage layer. It composes `board` pin definitions with
the `sd_card` wrapper and owns app SD-card mount and format state.

Current scope:

- use the schematic page 5 MicroSD pin map
- check `SD_DETECT`
- mount `/sdcard` during boot when a card is present
- keep the card mounted during normal runtime after successful boot-time init
- format the SD card on request, set the `FOLLOUP` volume label, and recreate
  the source-app directory layout:
  `/recordings`, `/todos`, `/summaries`, `/files`, `/trash`,
  `/trash/recordings`, and `/trash/todos`
- publish coarse format lifecycle state only: started, succeeded, or failed
- avoid progress-checkpoint UI churn during format; the current product flow
  shows a single "Formatting in progress. Please wait..." modal until the
  operation completes with success or error
- log mount status, total/free bytes, and a small root directory preview
- write/read `/sdcard/SDPROBE.TXT` once as a bring-up probe

An absent SD card is not a fatal app startup error. Mount failures are logged
and returned to AppShell as non-fatal service initialization failures.

MicroSD shares SPI lines with the e-paper path:

- `SD_CLK/SCK` / `EP_SCK`: `GPIO13`
- `SD_CMD/MOSI` / `EP_SDI`: `GPIO14`
- `SD_D0/MISO`: `GPIO12`
On this board the SD card uses the SDMMC controller and the panel owns SPI3, so
there is no shared bus to arbitrate and no bus guard to acquire. The Sticky's
shared-SPI ordering rules do not apply here.

During runtime SD format on Sticky, the storage path should minimize shared-SPI
display activity. The current product policy is to show the formatting overlay
once when format begins and then leave the SD worker undisturbed until the
success or error modal is shown at the end. Do not reintroduce intermediate
format-progress overlay refreshes unless a hardware-validated need outweighs
the added bus contention risk.

Hardware validation on Sticky showed an extra board-specific constraint: when an
SD card is inserted, the card must be initialized on the shared SPI bus before
the e-paper panel starts using that bus, and the card should remain mounted
afterward. Tearing the card back down after boot caused the panel to log a
refresh without visibly updating the screen. Treat "SD first, then display, and
keep SD mounted" as a required startup policy on this hardware revision.

### `components/playback_service`

This component streams a clip to the codec. It is deliberately dumb: no policy,
no task ownership, no state beyond "am I playing".

Current scope:

- `PlayFile(path)` streams a 16 kHz mono 16-bit PCM WAV from SD
- `PlayClip(clip)` streams the PSRAM chunks `recording_service` already holds,
  with no SD round-trip
- `IsPlaying()` / `Stop()` for callers that need to interrupt

Both entry points block for the length of the clip, so callers run them on a
short-lived worker task. `PlayClip` exists for the review step after recording:
at that point the take has deliberately not been written to SD yet, so a clip the
user discards never touches the card.

### `components/recording_service`

This is the app-facing voice-input recording layer. It composes the `audio_hal`
codec capture side with app policy for pre-roll, recording state, clip
ownership, input-level telemetry, and WAV file output.

Current scope:

- create a dedicated capture task that reads short PCM chunks from the codec
- keep a one-second PSRAM-backed pre-roll ring buffer while armed
- support starting a recording with or without pre-roll
- store the active clip in PSRAM-backed chunks with a 10-second max duration
- track a simple input-level percentage for UI/debug/VAD preparation
- expose `Arm()`, `Start()`, `Finish()`, `Cancel()`, `DiscardClip()`, and
  `GetRecordedClip()`
- save the latest clip as a mono 16-bit PCM WAV file on MicroSD

The service does not implement playback. It owns the clip and hands it out via
`GetRecordedClip()`; `playback_service` is what streams one to the codec. That
split is deliberate — the recording layer should not grow an output path, and
`playback_service` should not know how a clip was produced.

Future voice-product work should build VAD, upload/transcription, and display
status on top of this service rather than pushing those policies down into
`audio_hal`.

### `components/epaper_panel`

This is the raw mono SSD1677 e-paper panel driver ported from:

```text
/Users/tieuvong/Development/followup/components/board_drivers/epaper_panel
```

The driver should stay board-agnostic. It receives an `EpaperPanelConfig` from
its caller and owns:

- e-paper reset, busy, data/command, and chip-select GPIO control
- the SSD1677 command/data write path
- the mono framebuffer
- the retained previous framebuffer (a shadow of what is on the glass) used by the
  partial-refresh differential
- full base refresh
- change-detected whole-screen partial refresh (diff the framebuffer against the shadow,
  drive only the changed pixels)
- panel sleep
- refresh timing metrics

Current scope is intentionally mono-only. Do not port gray4 support unless a
future product requirement explicitly asks for it.

The first display update must use `RefreshFullBase()` so the SSD1677 current
(`0x24`) and previous (`0x26`) RAM planes are seeded. Per-interaction updates call
`RefreshChangedRegion()`, which diffs the freshly rendered framebuffer against the
retained shadow and, only if something changed, drives a whole-screen partial via
`RefreshPartialFullScreen()`. The differential waveform physically moves only the
pixels that differ, so a whole-screen partial still updates just the changed element
with no flash. If partial refresh is requested before a base image exists, after
sleep/timeout, or after the partial-refresh limit, the driver falls back to
`RefreshFullBase()`.

> **Windowed / region partial refresh is NOT supported on this SSD1677 (GDEM0397T81)
> panel.** The master activation drives the *whole* panel from the `0x24` plane — the
> RAM window registers (`0x44/0x45`) scope only where writes land, not where the panel
> is driven, and there is no register to limit the drive to a window. So a windowed
> write leaves stale RAM outside it that gets re-energized on the next activation
> (previously-focused elements "relight"). This was proven three ways: empirically,
> against the datasheet, and with a from-scratch isolation test. Only full-buffer writes
> are coherent, so every partial rewrites both RAM planes in full. `RefreshPartialRegion()`
> remains internally but is only ever called with full-panel bounds. The driver also
> applies a Y gate-line mapping fix (`window_y = height-1-raw_y`) and removes the
> per-partial hardware reset (the datasheet resets only at power-on).

The driver can initialize its own SPI bus, which is what this board does: the
panel is given `external_spi_bus=false` and manages `SPI3_HOST` itself, write-only
with no MISO.

Not yet ported from Folloup:

- wake API and display wake policy
- fast refresh/base path
- logical-to-raw display view abstraction

(A retained view dirty-region / windowed partial-refresh policy is intentionally **not**
pursued: the SSD1677 panel cannot drive a sub-window, so region partial refresh is not
viable here — see the driver note above.)

### `components/display_service`

This is the app-facing display layer. It composes `board` pin definitions and
power helpers with the `epaper_panel` raw driver.

Current scope:

- initialize the raw panel on its dedicated SPI3 bus (the panel is fed by the
  AXP2101 rails, so there is no GPIO power-enable to assert)
- initialize the raw SSD1677 panel driver
- render the startup splash with `RefreshFullBase()`
- own the current portrait framebuffer surface and its refresh policy
- render the current active screen (the dashboard home, onboarding, any feature
  page, or the lock screen) together with the appropriate UI chrome
- enter panel sleep without a special transitional text screen
- restore the current screen with a forced full refresh after display wake or
  light-sleep recovery
- log panel refresh metrics and expose refresh-in-progress state for
  auto-sleep blocking

Current UI state:

- `display_service` owns the `ScreenId` screen model: the dashboard home,
  onboarding, the feature pages (vibe check, summarize, notes, todos, follow-up,
  details, settings, wifi, time), and a real lock screen
- the status bar is now rendered through `epaper_ui`
- the global footer is rendered through `epaper_ui` and fed by
  `main/footer_runtime.cpp`
- the lock screen uses its own `epaper_ui` renderer and a dedicated runtime
  helper in `main/lock_screen_runtime.cpp`
- overlays are composited on top of the active screen in `DrawCurrentOverlays`
  (z-order keyboard → toast → select modal → card modal → sticky note); the
  `RenderSnapshot` carries each overlay's state (`card_modal`, `select_modal`,
  `keyboard`, `toast`, `sticky_note`)
- overlay presentation has two app-facing refresh paths:
  - show/hide or footprint changes rebuild the underlay before redrawing the
    overlay
  - same-visibility overlay churn such as roving-focus updates or sticky-note
    scrolling may reuse the cached underlay snapshot
- sleep and shutdown indicators are driven through `status_bar_runtime`
  immediately before display sleep, light sleep, and deep-sleep shutdown
  transitions

Current decoupled refresh rule:

- mutate runtime state first in the owning runtime helper
- schedule keyed UI presentation work through `main/ui_refresh_runtime.cpp`
- let `ui_refresh_runtime` coalesce stale intermediate updates and keep the
  latest state for each keyed surface while the panel is busy
- carry the refresh mode through that queue as a `display_service::RefreshRequest`
  (partial vs full); partials are change-detected whole-screen, not windowed
- keep page-owned focus refresh on that same queue instead of bouncing through
  an extra app-shell UI dispatcher layer
- keep `display_service` as the sole owner of framebuffer mutation and panel
  refresh execution

The current keyed surfaces are:

- overlay
- lock screen
- status bar
- footer
- one per page: dashboard, onboarding, vibe check, summarize, notes, todos,
  follow-up, details, settings, WiFi, time

Current refresh categories are:

- whole-screen partial refresh: the default for every in-screen update (focus roving,
  state churn, overlay reuse). The driver diffs the rendered frame against the shadow
  and the differential waveform moves only the changed pixels — there is no
  windowed/region variant because the panel cannot drive a sub-window (see the driver
  note above)
- full base refresh: used for explicit full refresh requests, wake recovery, the
  periodic ghost-clear cadence, and other panel-reset cases

The current focus path is:

- page input mutates page-owned focus truth first
- the page runtime flags only whether the visible footer projection actually changed
  (it no longer computes per-interaction dirty bounds — that machinery was removed once
  region refresh proved unviable)
- `ui_refresh_runtime` coalesces the latest `RefreshRequest` for that page
- when footer projection changed, the queued page apply updates page state and
  footer state together once before the refresh reaches the panel queue
- `display_service` re-renders the active screen and lets the driver diff it against
  the shadow, driving a whole-screen partial of only the changed pixels (or skipping
  entirely when nothing changed)
- overlays (keyboard, modals) refresh through the overlay path: while typing, only the
  cached underlay is reused and the overlay redrawn — the page underneath is **not**
  re-rendered until the overlay closes
- page-entry transitions still stay synchronous in `app_shell` rather than
  going through the latest-wins queue, because the app shell must preserve
  deterministic ordering for touch-provider setup, footer layout, runtime state
  sync, and screen switch

The temporary demo-selection machinery has been removed; `display_service` owns a
`ScreenId`-based screen model. The home screen renders the real dashboard
(`DrawHomeUnderlay` → `epaper_ui::DrawDashboardPage`), whose focusable menu opens
the feature pages.

Because the SSD1677 path shares `SPI2_HOST` with MicroSD, `display_service`
depends on `storage_service` having already performed SD bring-up when a card is
inserted. The display path should not reorder itself ahead of storage during
boot on this board.

`display_service` owns app-facing display policy. Driver-specific wiring and
SSD1677 commands must stay out of `main`. Raw board pin ownership stays in
`board`, and low-level SSD1677 command sequencing stays in `epaper_panel`.

Port validation notes still pending on hardware:

- rapid select-modal roving should continue to log
  `policy=reuse_underlay_snapshot` and must not leave stale highlight pixels
- modal and toast show/hide transitions should rebuild cleanly without ghosting
- the footer mic icon should stay active through recording, saving, and
  transcribing states
- overlay behavior should remain correct across display sleep and light-sleep
  wake
- touch-down focus should be visible before release-based activation for select
  modal rows, shutdown buttons, and footer items
- overlay, footer, and future page precedence logs should match the touched
  surface during on-device validation

### `components/qmi8658`

This is the QMI8658 6-axis IMU driver (3-axis accelerometer plus 3-axis
gyroscope), on the shared sensor I2C bus.

Current scope:

- bring-up and configuration
- read accelerometer samples used by `device_sleep_service` for motion wake
- keep sleep policy and thresholds out of the driver

### `components/imu_service`

This is the app-facing IMU layer. It composes `board` I2C access with the
generic QMI8658 driver.

Current scope:

- add the QMI8658 on the shared sensor I2C bus
- configure the accelerometer and read samples
- expose samples to `device_sleep_runtime`, which owns motion/still
  classification and the inactivity thresholds

The IMU shares the sensor I2C bus with the PMIC and RTC. Auto-sleep
intentionally polls rather than attaching an interrupt, so there is no shared
interrupt line to coordinate.

## Hardware Notes

- Main controller: `ESP32-S3R8`, dual-core Xtensa LX7 up to 240 MHz.
- External flash: 16 MB. PSRAM: 8 MB.
- Shared sensor I2C on `GPIO41` (SDA) / `GPIO42` (SCL), carrying:
  - AXP2101 PMIC at `0x34`, interrupt on `GPIO38`
  - PCF85063 RTC at `0x51`
  - QMI8658 IMU
  - ES8311 codec control interface
  - SHTC3 temperature/humidity, present but not driven by any component
- Neither I2C pin is a strapping pin, so the bus can be created during early
  startup without affecting boot mode.
- Buttons are all active-low to GND: `ACTION`/BOOT on `GPIO0`, rocker up on
  `GPIO4`, rocker middle/`FN` on `GPIO5`, rocker down on `GPIO6`. `GPIO0` is the
  boot/download strap, so it must read high at reset and is only ever pulled low
  by a press.
- The `PWR` key is not a GPIO. It is wired to the AXP2101 and surfaces as
  interrupts: a short-press IRQ, a long-press IRQ once held past `IrqLevelTime`,
  and a hardware rail cut at a sustained 6 s hold.
- MicroSD uses the ESP32-S3 SDMMC controller in 4-bit mode: `CLK` on `GPIO16`,
  `CMD` on `GPIO17`, `D0` on `GPIO15`, `D1` on `GPIO7`, `D2` on `GPIO8`, `D3` on
  `GPIO18`. There is no card-detect or power-enable pin.
- The SSD1677 e-paper panel owns a dedicated `SPI3_HOST`, write-only with no
  MISO: `BUSY` on `GPIO3`, `DC` on `GPIO9`, `CS` on `GPIO10`, `SCK` on `GPIO11`,
  `MOSI` on `GPIO12`, `RST` on `GPIO46`. The panel is fed by the AXP2101 rails,
  so there is no GPIO power-enable.
- Because the panel does not share a bus with MicroSD, none of the Sticky's
  shared-SPI serialization applies here: there is no bus guard, and no ordering
  requirement between mounting SD and bringing up the display.
- The e-paper panel is 800 x 480 raw landscape pixels. `display_service` draws
  portrait content by mapping logical 480 x 800 coordinates into the raw
  framebuffer. Note that portrait `x` maps onto the panel's gate line
  (`raw_y = height - 1 - x`), which is why a large fill with a gate-periodic
  dither pattern produces visible banding on a partial refresh.
- ES8311 audio streams over I2S0 at 16 kHz full duplex: `MCLK` on `GPIO13`,
  `BCLK` on `GPIO14`, `WS` on `GPIO47`, `DIN` on `GPIO21`, `DOUT` on `GPIO48`.
  The NS4150B power-amp enable is on `GPIO39`.
- There is no power latch, no charger-enable GPIO, and no ADC power-input sense.
  The AXP2101 owns rails, charging, and battery telemetry over I2C.

## Configuration

Configuration is file-based and should stay reproducible:

- `sdkconfig.defaults` captures the intended project defaults.
- `sdkconfig` captures the resolved ESP-IDF configuration.
- `partitions.csv` defines the OTA partition table.

Project-specific Kconfig options live under `Folloup Settings`. Auto-sleep
currently exposes reproducible build-time defaults for display sleep and light
sleep timeout seconds; `0` disables the corresponding stage, and a nonzero light
sleep timeout must be greater than or equal to the display sleep timeout.

The partition table currently contains:

- `nvs`
- `otadata`
- `phy_init`
- `ota_0`
- `ota_1`

Rollback is enabled with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Because
rollback is enabled, the application must keep the OTA validation hook in
`app_main()` or equivalent early startup code.

## Dependency Direction

Use this dependency direction:

```text
app / integration code
  -> power_service
       -> board -> ESP-IDF drivers
       -> axp2101 -> ESP-IDF I2C/GPIO drivers
       -> pcf85063 -> ESP-IDF I2C driver
  -> power_key_runtime (main/)
       -> board (GetPmic)
       -> axp2101
       -> device_sleep_service / device_sleep_runtime
  -> button_service -> espressif/button
  -> feedback_service
       -> system_sound_service -> audio_hal -> ESP-IDF I2S driver
  -> device_sleep_service
  -> storage_service
       -> sd_card -> ESP-IDF SDMMC/FATFS drivers
  -> recording_service
       -> board (GetAudioCodec)
       -> audio_hal -> ESP-IDF I2S/I2C drivers
  -> playback_service
       -> board (GetAudioCodec)
       -> audio_hal
       -> recording_service (RecordedClip only)
  -> recording_session_service
       -> recording_service
       -> playback_service
       -> system_sound_service
  -> display_service
       -> epaper_panel -> ESP-IDF SPI/GPIO drivers
  -> imu_service
       -> qmi8658 -> ESP-IDF I2C driver
```

Avoid making `axp2101`, `pcf85063`, `sd_card`, `epaper_panel`, `qmi8658`, or
`audio_hal` depend on `board`; that would make generic drivers board-specific.
`board` composes them with Waveshare pin mapping, and app-facing services reach
the hardware through `board` accessors such as `GetPmic()` and
`GetAudioCodec()`.

`playback_service` depends on `recording_service` only for the `RecordedClip`
type it streams. The reverse edge must not exist: `recording_service` owns
capture and clip ownership, and does not gain an output path.
