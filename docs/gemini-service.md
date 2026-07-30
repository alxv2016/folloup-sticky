# Gemini Service

This document describes the current Gemini integration used by the Followup
firmware.

`components/gemini_service/` currently owns:

- Gemini API key storage precedence
- Gemini authentication state
- backend HTTP routes for Gemini settings and runtime state
- Wi-Fi-driven Gemini readiness
- Gemini-ready status consumed by the status bar

It does not own:

- transcription requests — owned by the `transcription_service` component
- summary generation — owned by the `summary_service` component
- archived recording enrichment / indexing — owned by
  `recording_archive_service`
- a frontend portal UI

`transcription_service` and `summary_service` call the Gemini API directly; they
were split into their own components rather than living inside `gemini_service`,
which remains focused on key/settings precedence, auth readiness, and status.

## Ownership

Current runtime split:

- `gemini_service`
  - Gemini configuration, API key precedence, auth requests, auth state, and
    backend route handlers
- `wifi_service`
  - owns the backend HTTP server and hosts Gemini routes through the existing
    portal route registrar
- `main/app_shell.cpp`
  - initializes the service, forwards Wi-Fi network state into Gemini, logs
    Gemini events, and triggers the Gemini-connected sound cue
- `main/status_bar_runtime.cpp`
  - reflects Gemini-ready state into `epaper_ui::StatusBarState`
- `components/epaper_ui/`
  - renders the Gemini-ready star icon in the status bar
- `feedback_service` / `system_sound_service`
  - play a dedicated Gemini-connected cue when readiness transitions from false
    to true

`app_shell` remains an orchestrator here. The Gemini service owns the provider
state and route behavior; `app_shell` only wires events, startup order, and
product-facing reactions.

## Internal Layout

Followup currently uses a single-file Gemini service implementation:

- [`components/gemini_service/include/gemini_service.h`](/Users/tieuvong/Development/folloup-sticky/components/gemini_service/include/gemini_service.h)
- [`components/gemini_service/gemini_service.cpp`](/Users/tieuvong/Development/folloup-sticky/components/gemini_service/gemini_service.cpp)

Current internal responsibilities inside that component:

- NVS read/write of the stored Gemini API key
- sdkconfig fallback key lookup
- effective-key precedence resolution
- HTTP `GET` model authentication against Gemini
- auth task lifecycle and stale-result protection
- backend JSON request/response handling

Followup does not yet split this into separate `client`, `worker`, and
`settings_storage` files the way Followup does.

## API Key Sources

Current API key precedence:

1. NVS-stored key saved through the backend API
2. built-in `CONFIG_FOLLOWUP_GEMINI_API_KEY`
3. no key configured

Resetting Gemini settings clears only the stored NVS key. It does not clear the
built-in sdkconfig fallback key.

## Build-Time Configuration

The build-time Gemini setting lives under `Folloup Settings`:

- `CONFIG_FOLLOWUP_GEMINI_API_KEY`

This is intended for development and bench testing. A key saved through the
backend API takes precedence over the built-in key.

The reproducible default is set in:

- [`sdkconfig.defaults`](/Users/tieuvong/Development/folloup-sticky/sdkconfig.defaults)

Current default:

- `CONFIG_FOLLOWUP_GEMINI_API_KEY=""`

## Startup And Readiness Flow

Current auth flow:

1. `app_shell` initializes `gemini_service`
2. the service loads the stored NVS key, if present
3. `wifi_service` reaches a connected STA state
4. `app_shell` forwards network state into `gemini_service::SetNetworkState(...)`
5. if a Gemini API key is available and no auth is already in flight, Gemini
   starts authentication automatically
6. Gemini performs a model `GET` against
   `https://generativelanguage.googleapis.com/v1beta/`
7. successful auth marks the runtime snapshot ready
8. the status bar shows the Gemini-ready star icon
9. the feedback layer plays the Gemini-connected cue the first time readiness
   transitions to true

Authentication is skipped when:

- no API key is configured
- a request is already in flight
- Wi-Fi is not connected
- Wi-Fi is in access-point mode

Followup currently treats `ready` as:

- `configured == true`
- `authenticated == true`

## Transport Details

The current auth path is intentionally minimal:

- request type: HTTP `GET`
- endpoint: `v1beta/<model>`
- default model: `models/gemini-2.5-flash-lite`
- auth header: `x-goog-api-key: <api_key>`

The current implementation uses ESP-IDF's `esp_http_client` with the CRT bundle
for TLS validation.

Followup does not yet implement:

- Gemini file upload
- transcription prompts
- text generation
- token counting

## Public C++ Snapshot Shapes

Source of truth:

- [`components/gemini_service/include/gemini_service.h`](/Users/tieuvong/Development/folloup-sticky/components/gemini_service/include/gemini_service.h)

### `gemini_service::SettingsSnapshot`

Current fields:

- `configured`
- `has_stored_api_key`
- `has_sdkconfig_api_key`
- `api_key_source`
- `api_key_last4`
- `model_name`

### `gemini_service::RuntimeSnapshot`

Current fields:

- `initialized`
- `ready`
- `request_in_flight`
- `auth_checked`
- `authenticated`
- `supports_audio_understanding`
- `supports_structured_output`
- `last_http_status`
- `last_status_message`
- `last_model_resource_name`
- `last_model_display_name`
- `last_error_code`
- `last_error_message`

`supports_audio_understanding` and `supports_structured_output` currently remain
`false` because This firmware has not yet ported those provider capabilities.

### `gemini_service::Snapshot`

Contains:

- `settings`
- `runtime`

### `gemini_service::SettingsPatch`

Current fields:

- `has_api_key`
- `api_key`

### `gemini_service::Result`

Current fields:

- `success`
- `validation_error`
- `status_code`
- `field`
- `error_code`
- `message`

## Backend Endpoints

Current backend endpoints:

- `GET /api/settings/gemini`
- `PATCH /api/settings/gemini`
- `POST /api/settings/gemini/reset`
- `GET /api/runtime/gemini`

These routes are registered through the existing Wi-Fi backend server. Gemini
does not start its own HTTP server.

Constraints:

- content type is JSON
- `PATCH` requires a JSON object body
- `PATCH` body must be greater than `0` bytes and at most `512` bytes

## JSON Contracts

### `PATCH /api/settings/gemini`

Current request shape:

```json
{
  "api_key": "AIza..."
}
```

Current validation rules:

- `api_key` must be present
- `api_key` must be a non-empty string after trimming

### Snapshot Responses

Used by:

- `GET /api/settings/gemini`
- `GET /api/runtime/gemini`
- successful `PATCH /api/settings/gemini`
- successful `POST /api/settings/gemini/reset`

Current response shape:

```json
{
  "success": true,
  "message": "Gemini settings loaded",
  "settings": {
    "configured": true,
    "has_stored_api_key": false,
    "has_sdkconfig_api_key": true,
    "api_key_source": "sdkconfig",
    "api_key_last4": "1234",
    "model_name": "models/gemini-2.5-flash-lite"
  },
  "runtime": {
    "initialized": true,
    "ready": true,
    "request_in_flight": false,
    "auth_checked": true,
    "authenticated": true,
    "supports_audio_understanding": false,
    "supports_structured_output": false,
    "last_http_status": 200,
    "last_status_message": "Authenticated with Gemini 2.5 Flash-Lite",
    "last_model_resource_name": "models/gemini-2.5-flash-lite",
    "last_model_display_name": "Gemini 2.5 Flash-Lite",
    "last_error_code": "",
    "last_error_message": ""
  }
}
```

Notes:

- `api_key_last4` is intentionally masked metadata only
- the raw API key is never returned by the backend
- `message` varies by endpoint and outcome

### Error Responses

Current error response shape:

```json
{
  "success": false,
  "message": "Gemini API key is required",
  "error_code": "missing_api_key",
  "field": "api_key"
}
```

Common current error codes:

- `missing_api_key`
- `invalid_api_key`
- `nvs_write_failed`
- `nvs_clear_failed`
- `not_configured`
- `task_alloc_failed`
- `task_start_failed`
- provider error codes returned from Gemini HTTP error payloads when present

## UI Integration

Followup currently uses Gemini readiness in two product-facing ways:

- the status bar shows the star icon when Wi-Fi is connected and Gemini is
  authenticated
- the feedback layer plays a dedicated Gemini-connected cue when readiness
  transitions from false to true

The current status-bar icon is the star asset already present in
`project_assets`. This firmware is not yet using the broader upstream Gemini-specific
page/UI flows.

## Logging

Current logs cover:

- service initialization
- auth start
- auth success
- auth failure
- stale auth result suppression
- backend route registration failures
- NVS read/write/clear failures
- app-facing Gemini event snapshots in `app_shell`

These logs are intended to make bring-up and backend integration easier before
the larger Gemini feature set is ported.

## Deferred Followup Features

The upstream Followup Gemini stack is broader than this implementation.

Not yet ported:

- transcription jobs
- summary generation
- token counting
- archived audio upload
- provider worker queue shared across multiple Gemini job types
- frontend portal UI for Gemini settings

When those features are ported, this document should be expanded rather than
replaced so it remains accurate for this firmware's actual runtime behavior at each
stage.
