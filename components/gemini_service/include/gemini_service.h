#ifndef GEMINI_SERVICE_H_
#define GEMINI_SERVICE_H_

#include <cstdint>
#include <string>

#include "esp_err.h"
#include "esp_http_server.h"

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

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

Result ApplySettingsPatch(const SettingsPatch& patch);
Result ClearStoredApiKey();

bool HasApiKey();
std::string GetEffectiveApiKey();
std::string GetEffectiveModelName();
bool BeginAuthentication();
void SetNetworkState(bool connected, bool access_point_mode);
void RegisterPortalRoutes(httpd_handle_t server);

const char* ApiKeySourceName(ApiKeySource source);

}  // namespace gemini_service

#endif  // GEMINI_SERVICE_H_
