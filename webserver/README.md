# Followup Setup Portal

Web UI for provisioning a Followup device over its Wi-Fi access point. The portal
is built with TypeScript + Vite and **embedded into the firmware** (see
[Build & Deploy](#build--deploy-to-firmware)) — it is served by the
`wifi_service` component's HTTP server while the device is in AP mode.

It is adapted from the byte90 captive portal, stripped down to the three things
Followup provisions:

- **Wi-Fi** — scan, connect, disconnect, and live status
- **Gemini API key** — save / clear
- **Time & timezone** — pick a timezone, or set date/time manually

The frontend is organized around:

- autonomous Web Components in `src/components/`
- feature controllers in `src/portal/`
- a thin `src/main.ts` bootstrap/orchestration layer

## API Endpoints Used By The Portal

These are served by the Followup firmware (`wifi_service`, `timezone_service`,
`gemini_service`) while the device is in access-point mode.

WiFi (`wifi_service`)

- `GET /api/scan`
- `GET /api/status`
- `POST /api/configure`
- `POST /api/disconnect`

Time / timezone (`timezone_service`)

- `GET /api/timezone/list`
- `GET /api/settings/time`
- `PATCH /api/settings/time`
- `GET /api/runtime/time`

Gemini (`gemini_service`)

- `GET /api/settings/gemini`
- `PATCH /api/settings/gemini`
- `POST /api/settings/gemini/reset`

## Local Development

From repo root:

```bash
cd webserver
npm install
npm run dev
```

Vite serves the app at `http://localhost:5173/`. The portal makes absolute
`/api/...` calls, so a dev server alone has no backend — point your machine at a
Followup device's AP (or proxy `/api` to one) to exercise the flows against real
firmware.

## Build & Deploy To Firmware

The firmware embeds a small, fixed set of files (`index.html`, `index.js`,
`index.css`) via `EMBED_FILES` in `components/wifi_service/CMakeLists.txt`. To
refresh the portal after any frontend change:

```bash
cd webserver
npm run build
cp dist/index.html dist/index.js dist/index.css ../components/wifi_service/portal/
```

Then rebuild the firmware. `wifi_service` serves them at `/`, `/index.js`, and
`/index.css`.

`webserver/` is the source of truth. The copies under
`components/wifi_service/portal/` are the tracked, embedded build output — refresh
them with the command above rather than hand-editing.

## Project Structure

```text
webserver/
  index.html              # portal markup (WiFi / Gemini / Time cards)
  src/assets/             # inline SVGs (?raw) incl. followup_logo.svg
  src/components/         # autonomous Web Components
  src/portal/             # feature controllers, API helpers, DOM wiring, types
  src/gradualBlur.ts
  src/main.ts             # bootstrap and orchestration
  src/portal.css          # global page/layout styles
  dist/                   # build output (generated, gitignored)
components/wifi_service/portal/   # embedded build output served by the firmware
```

## Portal Source Layout

- `src/components/`
  Shared Web Components such as `Button`, `Input`, `Select`, `Card`, `Toast`,
  `NetworkStatus`, `NetworkList`, and `BottomSheet`.

- `src/portal/`
  Portal-specific application logic split by responsibility:
  - `api.ts` — typed API fetch helpers
  - `wifi.ts` — WiFi scan/connect/disconnect flow
  - `time.ts` — timezone + manual date/time
  - `providerKeys.ts` — Gemini API key save/clear
  - `dom.ts` — element lookups
  - `events.ts` — DOM event bindings
  - `uiState.ts` — button/input enabled/disabled state
  - `duration.ts`, `uiHelpers.ts` — shared helpers
  - `constants.ts`, `types.ts` — shared constants and TypeScript types

## Scripts

- `npm run dev` — start the Vite dev server
- `npm run build` — type-check (`tsc`) and build production assets into `dist/`
- `npm run preview` — preview the production build
- `npm run lint` — run ESLint
