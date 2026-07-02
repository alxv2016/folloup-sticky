#ifndef GEMINI_SERVICE_H_
#define GEMINI_SERVICE_H_

#include <cstdint>
#include <string>

#include "esp_err.h"
#include "esp_http_server.h"

namespace recording_service {
class RecordedClip;
}

namespace gemini_service {

enum class ApiKeySource : uint8_t {
    kNone = 0,
    kSdkConfig,
    kNvs,
};

struct SettingsSnapshot {
    bool configured = false;
    bool has_stored_api_key = false;
    bool has_sdkconfig_api_key = false;
    ApiKeySource api_key_source = ApiKeySource::kNone;
    std::string api_key_last4;
    std::string model_name;
};

struct RuntimeSnapshot {
    bool initialized = false;
    bool ready = false;
    bool request_in_flight = false;
    bool auth_checked = false;
    bool authenticated = false;
    bool supports_audio_understanding = false;
    bool supports_structured_output = false;
    int last_http_status = 0;
    std::string last_status_message;
    std::string last_model_resource_name;
    std::string last_model_display_name;
    std::string last_error_code;
    std::string last_error_message;
};

struct Snapshot {
    SettingsSnapshot settings = {};
    RuntimeSnapshot runtime = {};
};

struct Event {
    Snapshot snapshot = {};
};

struct SettingsPatch {
    bool has_api_key = false;
    std::string api_key;
};

struct Result {
    bool success = false;
    bool validation_error = false;
    int status_code = 500;
    std::string field;
    std::string error_code;
    std::string message;
};

// Result of a synchronous text-generation (generateContent) call.
struct TextResult {
    bool success = false;
    int http_status = 0;
    std::string text = {};
    std::string error_code = {};
    std::string error_message = {};
};

// Result of a synchronous token-count (countTokens) call.
struct TokenCountResult {
    bool success = false;
    int http_status = 0;
    int total_tokens = 0;
    std::string error_code = {};
    std::string error_message = {};
};

// Result of a synchronous audio transcription (resumable upload + generateContent) call.
struct TranscriptionResult {
    bool success = false;
    int http_status = 0;
    std::string transcript = {};
    std::string error_code = {};
    std::string error_message = {};
    uint32_t clip_duration_ms = 0;
    size_t wav_bytes = 0;
    uint32_t upload_chunk_count = 0;
    uint64_t upload_elapsed_ms = 0;
    uint64_t total_elapsed_ms = 0;
};

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

Result ApplySettingsPatch(const SettingsPatch& patch);
Result ClearStoredApiKey();

bool HasApiKey();
std::string GetEffectiveApiKey();
std::string GetEffectiveModelName();
// Synchronous Gemini calls (block on HTTP; run them from a worker task, never a UI/input
// task). They use the effective API key + model and return the parsed result or an error.
TextResult GenerateText(const std::string& prompt);
TokenCountResult CountTokens(const std::string& prompt);
TranscriptionResult Transcribe(const recording_service::RecordedClip& clip);
bool BeginAuthentication();
void SetNetworkState(bool connected, bool access_point_mode);
void RegisterPortalRoutes(httpd_handle_t server);

const char* ApiKeySourceName(ApiKeySource source);

}  // namespace gemini_service

#endif  // GEMINI_SERVICE_H_
