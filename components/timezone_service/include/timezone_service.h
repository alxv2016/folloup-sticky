#ifndef TIMEZONE_SERVICE_H_
#define TIMEZONE_SERVICE_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "esp_err.h"
#include "esp_http_server.h"

namespace timezone_service {

enum class TimeSource : uint8_t {
    kUnknown = 0,
    kRtc,
    kManual,
    kNtp,
};

struct TimezoneInfo {
    std::string name;
    std::string description;
};

struct SettingsSnapshot {
    bool enabled = false;
    std::string timezone_name;
    std::string location;
};

struct RuntimeSnapshot {
    bool clock_enabled = false;
    bool time_valid = false;
    TimeSource time_source = TimeSource::kUnknown;
    bool has_network_sync = false;
    bool sync_in_progress = false;
    uint32_t last_network_sync_epoch = 0;
    std::string current_date;
    std::string current_time;
};

struct Snapshot {
    SettingsSnapshot settings = {};
    RuntimeSnapshot runtime = {};
};

struct Event {
    Snapshot snapshot = {};
};

struct SettingsPatch {
    bool has_enabled = false;
    bool enabled = false;
    bool has_timezone_name = false;
    std::string timezone_name;
    bool has_location = false;
    std::string location;
    bool has_manual_datetime = false;
    std::string manual_date;
    std::string manual_time;
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
std::vector<TimezoneInfo> ListTimezones();
void SetNetworkConnected(bool connected);
Result ApplySettingsPatch(const SettingsPatch& patch);
bool SyncNow(const char* ntp_server = nullptr, uint32_t timeout_ms = 2000);
bool IsSyncInProgress();
void RegisterPortalRoutes(httpd_handle_t server);

const char* TimeSourceName(TimeSource source);

}  // namespace timezone_service

#endif  // TIMEZONE_SERVICE_H_
