# Gemini Service

This document describes the current Gemini integration used by the Sticky
firmware.

`components/gemini_service/` currently owns:

- Gemini model selection and API key storage precedence
- Gemini authentication state
- backend HTTP routes for Gemini settings and runtime state
- Wi-Fi-driven Gemini readiness
- Gemini-ready status consumed by the status bar

The service also owns Gemini HTTP requests for transcription, text generation,
and token counting. The transcription and summary components manage their
respective workflows and call the service for provider requests.

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
- `feedback_service` / `buzzer_service`
  - play a dedicated Gemini-connected cue when readiness transitions from false
    to true

`app_shell` remains an orchestrator here. The Gemini service owns the provider
state and route behavior; `app_shell` only wires events, startup order, and
product-facing reactions.

## Internal Layout

Sticky currently uses a single-file Gemini service implementation:

- [`components/gemini_service/include/gemini_service.h`](/Users/tieuvong/Development/folloup-sticky/components/gemini_service/include/gemini_service.h)
- [`components/gemini_service/gemini_service.cpp`](/Users/tieuvong/Development/folloup-sticky/components/gemini_service/gemini_service.cpp)

Current internal responsibilities inside that component:

- NVS read/write of the stored Gemini API key
- sdkconfig fallback key lookup
- effective-key precedence resolution
- HTTP `GET` model authentication against Gemini
- auth task lifecycle and stale-result protection
- backend JSON request/response handling

Sticky does not yet split this into separate `client`, `worker`, and
`settings_storage` files the way Followup does.

## API Key Sources

Current API key precedence:

1. NVS-stored key saved through the backend API
2. built-in `CONFIG_FOLLOWUP_GEMINI_API_KEY`
3. no key configured

Clearing the API key removes the NVS key and preserves the selected model and
any built-in fallback key.

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

Sticky currently treats `ready` as:

- `configured == true`
- `authenticated == true`

## Transport Details

The current Sticky auth path is intentionally minimal:

- request type: HTTP `GET`
- endpoint: `v1beta/<model>`
- default model: `models/gemini-3.5-flash-lite`
- auth header: `x-goog-api-key: <api_key>`

The current implementation uses ESP-IDF's `esp_http_client` with the CRT bundle
for TLS validation.

Transcription starts a resumable upload through the
[Files API](https://ai.google.dev/api/files), streams and finalizes a PCM WAV
(`audio/wav`), then sends the returned file URI as `fileData` to the selected
model's [generateContent endpoint](https://ai.google.dev/api/generate-content).
Summaries use the same model for text generation and token counting.

## Model selection and 404 diagnostics

Open the Sticky setup portal at `http://192.168.4.1/` while connected to its
setup Wi-Fi. Under **Gemini Model**, choose a preset or enter a custom model ID,
then select **Save model**. The model is stored in the `gemini` NVS namespace
under `model_name`. Upgraded devices with no stored model use Gemini 3.5
Flash-Lite. Changing the model refreshes the device's metadata check when
internet is available.

Google documents [Gemini 3.5 Flash-Lite](https://ai.google.dev/gemini-api/docs/models/gemini-3.5-flash-lite)
and [Gemini 3.8 Flash](https://ai.google.dev/gemini-api/docs/models/gemini-3.8-flash)
as audio-capable models with text output. Google restricts the 2.5 family to
projects with prior usage. The old hardcoded 2.5 Flash-Lite ID is therefore a
likely cause of a generation 404. [Google's model catalog](https://ai.google.dev/gemini-api/docs/models)
and [project eligibility explanation](https://discuss.ai.google.dev/t/gemini-2-5-flash-metadata-succeeds-but-generatecontent-returned-http-404-request-for-project-eligibility-clarification/197853/2)
describe the restriction.

The readiness icon follows a successful model metadata GET. Verify generation
by recording a short clip and checking for a transcript. Serial logs identify
upload start, upload finalization, or `generateContent` failures. A 404 during
upload points to the upload endpoint or session, while a 404 during generation
can refer to the model or the uploaded file. Use the accompanying provider
error to distinguish those cases.

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
`false` because Sticky has not yet ported those provider capabilities.

### `gemini_service::Snapshot`

Contains:

- `settings`
- `runtime`

### `gemini_service::SettingsPatch`

Current fields:

- `has_api_key`
- `api_key`
- `has_model_name`
- `model_name`

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

Alternatively, send a model-only patch:

```json
{
  "model_name": "gemini-3.8-flash"
}
```

Each patch updates one setting. API keys must be nonempty after trimming.
Model IDs may include the `models/` prefix and are returned with that prefix.
The firmware validates the ID syntax when saving. A recording verifies model
access and audio support.

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
    "model_name": "models/gemini-3.5-flash-lite"
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
    "last_status_message": "Authenticated with Gemini 3.5 Flash-Lite",
    "last_model_resource_name": "models/gemini-3.5-flash-lite",
    "last_model_display_name": "Gemini 3.5 Flash-Lite",
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

Sticky currently uses Gemini readiness in two product-facing ways:

- the status bar shows the star icon when Wi-Fi is connected and Gemini is
  authenticated
- the feedback layer plays a dedicated Gemini-connected cue when readiness
  transitions from false to true

The current status-bar icon is the star asset already present in
`project_assets`. Sticky is not yet using the broader Followup Gemini-specific
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

Followup's Gemini stack is broader than Sticky's current implementation.

Not yet ported:

- transcription jobs
- summary generation
- token counting
- archived audio upload
- provider worker queue shared across multiple Gemini job types
- frontend portal UI for Gemini settings

When those features are ported, this document should be expanded rather than
replaced so it remains accurate for Sticky's actual runtime behavior at each
stage.

## Build, flash, and check on a Sticky

Use ESP-IDF 5.5.4, the version referenced in the repository's editor
configuration. Install it for macOS using the
[ESP-IDF setup guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32s3/get-started/).
Activate it in your terminal using the installation's `export.sh`. The project's
default target is ESP32-S3. The generated portal assets are already embedded.

`Mac mini — project root` (`/Users/home/Documents/Codex/sticky`)

```sh
cd /Users/home/Documents/Codex/sticky
idf.py build
```

Connect the Sticky over USB and find its serial port.

`Mac mini — anywhere`

```sh
ls /dev/cu.usb*
```

Replace `/dev/cu.usbmodemXXXX` below with the Sticky's actual device name. Use the normal
flash command to preserve the existing NVS settings. Close the serial monitor
with Ctrl+].

`Mac mini — project root` (`/Users/home/Documents/Codex/sticky`)

```sh
cd /Users/home/Documents/Codex/sticky
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

1. Open the setup portal and confirm that the saved key is still masked. If
   needed, enter the API key directly in the portal.
2. Save a model, restart the Sticky, and reopen the portal to verify persistence.
3. Connect to Wi-Fi, record a short clip, and verify that a transcript appears.
4. Generate a summary and check the serial monitor if either operation fails.
