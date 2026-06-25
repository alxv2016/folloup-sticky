#include "timezone_service.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>
#include <mutex>
#include <string>
#include <sys/time.h>
#include <utility>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "power_service.h"
#include "sdkconfig.h"

namespace timezone_service {
namespace {

constexpr const char* kTag = "TimezoneService";
constexpr const char* kNvsNamespace = "timezone";
constexpr const char* kEnabledKey = "enabled";
constexpr const char* kTimezoneNameKey = "tz_name";
constexpr const char* kLocationKey = "location";
constexpr const char* kTimeSourceKey = "time_src";
constexpr const char* kNtpSyncedKey = "ntp_sync";
constexpr const char* kNtpEpochKey = "ntp_epoch";
constexpr const char* kDefaultNtpServer = "pool.ntp.org";
constexpr const char* kChinaNtpServer = "cn.pool.ntp.org";
constexpr time_t kMinValidEpoch = 1600000000;
constexpr size_t kMaxPortalPayloadLen = 512;
constexpr uint32_t kSyncTaskStackWords = 6144;
constexpr UBaseType_t kSyncQueueDepth = 4;
constexpr const char* kPortalApiSettingsTimeUri = "/api/settings/time";
constexpr const char* kPortalApiRuntimeTimeUri = "/api/runtime/time";
constexpr const char* kPortalApiTimezoneListUri = "/api/timezone/list";

struct TimezoneCatalogEntry {
    const char* name;
    const char* tz_string;
    const char* description;
};

struct TimezoneAliasEntry {
    const char* alias;
    const char* canonical;
};

struct SyncRequest {
    bool force = false;
};

constexpr TimezoneCatalogEntry kTimezones[] = {
    {"North_America_Eastern", "EST5EDT,M3.2.0,M11.1.0", "Eastern Time"},
    {"North_America_Central", "CST6CDT,M3.2.0,M11.1.0", "Central Time"},
    {"North_America_Mountain", "MST7MDT,M3.2.0,M11.1.0", "Mountain Time"},
    {"North_America_Pacific", "PST8PDT,M3.2.0,M11.1.0", "Pacific Time"},
    {"North_America_Alaska", "AKST9AKDT,M3.2.0,M11.1.0", "Alaska Time"},
    {"North_America_Hawaii", "HST10", "Hawaii Time"},
    {"UTC", "UTC0", "UTC"},
    {"UK", "GMT0BST,M3.5.0,M10.5.0", "United Kingdom"},
    {"Central_Europe", "CET-1CEST,M3.5.0,M10.5.0", "Central Europe"},
    {"Eastern_Europe", "EET-2EEST,M3.5.0,M10.5.0", "Eastern Europe"},
    {"Japan", "JST-9", "Japan"},
    {"China", "CST-8", "China"},
    {"Australia_Eastern", "AEST-10AEDT,M10.1.0,M4.1.0", "Australia Eastern"},
    {"Australia_Central", "ACST-9:30ACDT,M10.1.0,M4.1.0", "Australia Central"},
    {"Australia_Western", "AWST-8", "Australia Western"},
};

constexpr TimezoneAliasEntry kTimezoneAliases[] = {
    {"America/New_York", "North_America_Eastern"},
    {"America/Detroit", "North_America_Eastern"},
    {"America/Toronto", "North_America_Eastern"},
    {"US/Eastern", "North_America_Eastern"},
    {"EST5EDT", "North_America_Eastern"},
    {"America/Chicago", "North_America_Central"},
    {"America/Winnipeg", "North_America_Central"},
    {"US/Central", "North_America_Central"},
    {"CST6CDT", "North_America_Central"},
    {"America/Denver", "North_America_Mountain"},
    {"America/Edmonton", "North_America_Mountain"},
    {"US/Mountain", "North_America_Mountain"},
    {"MST7MDT", "North_America_Mountain"},
    {"America/Los_Angeles", "North_America_Pacific"},
    {"America/Vancouver", "North_America_Pacific"},
    {"US/Pacific", "North_America_Pacific"},
    {"PST8PDT", "North_America_Pacific"},
    {"America/Anchorage", "North_America_Alaska"},
    {"US/Alaska", "North_America_Alaska"},
    {"AKST9AKDT", "North_America_Alaska"},
    {"Pacific/Honolulu", "North_America_Hawaii"},
    {"US/Hawaii", "North_America_Hawaii"},
    {"HST10", "North_America_Hawaii"},
    {"UTC", "UTC"},
    {"Etc/UTC", "UTC"},
    {"Etc/GMT", "UTC"},
    {"Europe/London", "UK"},
    {"Europe/Berlin", "Central_Europe"},
    {"Europe/Paris", "Central_Europe"},
    {"Europe/Warsaw", "Central_Europe"},
    {"Europe/Athens", "Eastern_Europe"},
    {"Europe/Bucharest", "Eastern_Europe"},
    {"Europe/Moscow", "Eastern_Europe"},
    {"Asia/Tokyo", "Japan"},
    {"Asia/Shanghai", "China"},
    {"Asia/Hong_Kong", "China"},
    {"Asia/Singapore", "China"},
    {"Australia/Sydney", "Australia_Eastern"},
    {"Australia/Melbourne", "Australia_Eastern"},
    {"Australia/Brisbane", "Australia_Eastern"},
    {"Australia/Adelaide", "Australia_Central"},
    {"Australia/Darwin", "Australia_Central"},
    {"Australia/Perth", "Australia_Western"},
};

std::mutex s_mutex;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
QueueHandle_t s_sync_queue = nullptr;
TaskHandle_t s_sync_task = nullptr;
std::atomic<bool> s_sync_in_progress = false;
volatile bool s_sntp_sync_seen = false;
bool s_initialized = false;
bool s_network_connected = false;
bool s_enabled = false;
bool s_has_network_sync = false;
uint32_t s_last_network_sync_epoch = 0;
TimeSource s_time_source = TimeSource::kUnknown;
std::string s_timezone_name;
std::string s_location;

std::string TrimCopy(std::string value)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string FormatLocalDate(time_t epoch_seconds)
{
    if (epoch_seconds < kMinValidEpoch) {
        return {};
    }

    std::tm local_tm = {};
    localtime_r(&epoch_seconds, &local_tm);
    char buffer[11] = {};
    strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local_tm);
    return buffer;
}

std::string FormatLocalTime(time_t epoch_seconds)
{
    if (epoch_seconds < kMinValidEpoch) {
        return {};
    }

    std::tm local_tm = {};
    localtime_r(&epoch_seconds, &local_tm);
    char buffer[6] = {};
    strftime(buffer, sizeof(buffer), "%H:%M", &local_tm);
    return buffer;
}

const char* ResolveTimezoneAlias(const char* timezone_name)
{
    if (timezone_name == nullptr) {
        return nullptr;
    }
    for (const auto& alias : kTimezoneAliases) {
        if (std::strcmp(alias.alias, timezone_name) == 0) {
            return alias.canonical;
        }
    }
    return timezone_name;
}

const TimezoneCatalogEntry* FindTimezoneByName(const char* timezone_name)
{
    if (timezone_name == nullptr || timezone_name[0] == '\0') {
        return nullptr;
    }

    const char* resolved_name = ResolveTimezoneAlias(timezone_name);
    for (const auto& timezone : kTimezones) {
        if (std::strcmp(timezone.name, resolved_name) == 0) {
            return &timezone;
        }
    }
    return nullptr;
}

const char* PickDefaultNtpServer(const char* timezone_tz)
{
    if (timezone_tz != nullptr && std::strstr(timezone_tz, "CST-8") != nullptr) {
        return kChinaNtpServer;
    }
    return kDefaultNtpServer;
}

bool ApplyTimezoneByName(const std::string& timezone_name)
{
    const TimezoneCatalogEntry* timezone = FindTimezoneByName(timezone_name.c_str());
    if (timezone == nullptr || timezone->tz_string == nullptr || timezone->tz_string[0] == '\0') {
        return false;
    }

    setenv("TZ", timezone->tz_string, 1);
    tzset();
    return true;
}

Snapshot BuildSnapshotLocked()
{
    Snapshot snapshot = {};
    snapshot.settings.enabled = s_enabled;
    snapshot.settings.timezone_name = s_timezone_name;
    snapshot.settings.location = s_location;

    const time_t now = time(nullptr);
    snapshot.runtime.clock_enabled = s_enabled;
    snapshot.runtime.time_valid = now >= kMinValidEpoch;
    snapshot.runtime.time_source = s_time_source;
    snapshot.runtime.has_network_sync = s_has_network_sync;
    snapshot.runtime.sync_in_progress = s_sync_in_progress.load(std::memory_order_relaxed);
    snapshot.runtime.last_network_sync_epoch = s_last_network_sync_epoch;
    snapshot.runtime.current_date = FormatLocalDate(now);
    snapshot.runtime.current_time = FormatLocalTime(now);
    return snapshot;
}

void Notify()
{
    EventHandler handler = nullptr;
    void* context = nullptr;
    Event event = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_event_handler;
        context = s_event_context;
        event.snapshot = BuildSnapshotLocked();
    }

    if (handler != nullptr) {
        handler(event, context);
    }
}

bool LoadString(nvs_handle_t handle, const char* key, std::string* out)
{
    if (out == nullptr) {
        return false;
    }

    size_t size = 0;
    esp_err_t err = nvs_get_str(handle, key, nullptr, &size);
    if (err != ESP_OK || size <= 1) {
        out->clear();
        return false;
    }

    std::string value(size, '\0');
    err = nvs_get_str(handle, key, value.data(), &size);
    if (err != ESP_OK) {
        out->clear();
        return false;
    }
    if (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    *out = std::move(value);
    return true;
}

void LoadSettingsFromStorage()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        s_enabled = CONFIG_FOLLOWUP_TIME_SYNC_DEFAULT_ENABLED;
        s_timezone_name = CONFIG_FOLLOWUP_DEFAULT_TIMEZONE_NAME;
        s_location.clear();
        return;
    }

    uint8_t enabled = 0;
    if (nvs_get_u8(handle, kEnabledKey, &enabled) == ESP_OK) {
        s_enabled = enabled != 0;
    } else {
        s_enabled = CONFIG_FOLLOWUP_TIME_SYNC_DEFAULT_ENABLED;
    }

    LoadString(handle, kTimezoneNameKey, &s_timezone_name);
    if (s_timezone_name.empty()) {
        s_timezone_name = CONFIG_FOLLOWUP_DEFAULT_TIMEZONE_NAME;
    }
    LoadString(handle, kLocationKey, &s_location);
    nvs_close(handle);
}

bool SaveSettingsToStorageLocked()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS for timezone settings: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u8(handle, kEnabledKey, s_enabled ? 1 : 0);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, kTimezoneNameKey, s_timezone_name.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, kLocationKey, s_location.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save timezone settings: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void LoadTimeStatusFromStorage()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        s_time_source = TimeSource::kUnknown;
        s_has_network_sync = false;
        s_last_network_sync_epoch = 0;
        return;
    }

    uint8_t time_source = static_cast<uint8_t>(TimeSource::kUnknown);
    if (nvs_get_u8(handle, kTimeSourceKey, &time_source) == ESP_OK) {
        s_time_source = static_cast<TimeSource>(time_source);
    } else {
        s_time_source = TimeSource::kUnknown;
    }

    uint8_t ntp_synced = 0;
    s_has_network_sync =
        nvs_get_u8(handle, kNtpSyncedKey, &ntp_synced) == ESP_OK && ntp_synced != 0;

    if (nvs_get_u32(handle, kNtpEpochKey, &s_last_network_sync_epoch) != ESP_OK) {
        s_last_network_sync_epoch = 0;
    }
    nvs_close(handle);
}

bool SaveTimeStatusToStorageLocked()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS for timezone time status: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_u8(handle, kTimeSourceKey, static_cast<uint8_t>(s_time_source));
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, kNtpSyncedKey, s_has_network_sync ? 1 : 0);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, kNtpEpochKey, s_last_network_sync_epoch);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save timezone time status: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool SetSystemEpoch(time_t epoch_seconds, TimeSource source, bool preserve_network_sync)
{
    if (epoch_seconds < kMinValidEpoch) {
        return false;
    }

    timeval tv = {
        .tv_sec = epoch_seconds,
        .tv_usec = 0,
    };
    if (settimeofday(&tv, nullptr) != 0) {
        ESP_LOGE(kTag, "Failed to set system time");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_time_source = source;
        if (!preserve_network_sync) {
            s_has_network_sync = source == TimeSource::kNtp;
            s_last_network_sync_epoch =
                source == TimeSource::kNtp ? static_cast<uint32_t>(epoch_seconds) : 0;
        }
        if (!SaveTimeStatusToStorageLocked()) {
            ESP_LOGW(kTag, "Failed to persist time status");
        }
    }
    return true;
}

bool SetRtcFromEpoch(time_t epoch_seconds)
{
    if (epoch_seconds < kMinValidEpoch) {
        return false;
    }

    std::tm local_tm = {};
    localtime_r(&epoch_seconds, &local_tm);
    return power_service::WriteRtcTime(local_tm) == ESP_OK;
}

bool SetSystemTimeFromRtc()
{
    std::tm local_tm = {};
    if (power_service::ReadRtcTime(&local_tm) != ESP_OK) {
        return false;
    }

    local_tm.tm_isdst = -1;
    const time_t epoch_seconds = mktime(&local_tm);
    if (epoch_seconds < kMinValidEpoch) {
        return false;
    }

    return SetSystemEpoch(epoch_seconds, TimeSource::kRtc, true);
}

bool ParseLocalDateTime(const std::string& date_value,
                        const std::string& time_value,
                        time_t* epoch_seconds)
{
    if (epoch_seconds == nullptr) {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    if (std::sscanf(date_value.c_str(), "%d-%d-%d", &year, &month, &day) != 3 ||
        std::sscanf(time_value.c_str(), "%d:%d", &hour, &minute) != 2) {
        return false;
    }
    if (year < 2020 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }

    std::tm local_tm = {};
    local_tm.tm_year = year - 1900;
    local_tm.tm_mon = month - 1;
    local_tm.tm_mday = day;
    local_tm.tm_hour = hour;
    local_tm.tm_min = minute;
    local_tm.tm_sec = 0;
    local_tm.tm_isdst = -1;

    const time_t parsed_epoch = mktime(&local_tm);
    if (parsed_epoch < kMinValidEpoch) {
        return false;
    }
    *epoch_seconds = parsed_epoch;
    return true;
}

bool IsSystemTimeValidLocked()
{
    return time(nullptr) >= kMinValidEpoch;
}

bool ShouldSyncOnNetworkConnectedLocked()
{
    // Re-sync on every (re)connect rather than only when the clock is invalid/unsynced: the
    // device drops WiFi intermittently, so we prefer a fresh NTP fix each time the network
    // comes back. SetNetworkConnected gates this to the disconnected->connected transition
    // so frequent WiFi events while already connected don't queue redundant syncs.
    return s_enabled && !s_timezone_name.empty();
}

void OnSntpTimeSync(timeval* tv)
{
    (void)tv;
    s_sntp_sync_seen = true;
}

bool QueueSync(bool force)
{
    if (s_sync_queue == nullptr) {
        return false;
    }

    SyncRequest request = {.force = force};
    if (xQueueSend(s_sync_queue, &request, 0) != pdTRUE) {
        ESP_LOGW(kTag, "Time sync queue full");
        return false;
    }
    return true;
}

void SyncWorker(void*)
{
    SyncRequest request = {};
    while (true) {
        if (xQueueReceive(s_sync_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!request.force) {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (!s_network_connected || !ShouldSyncOnNetworkConnectedLocked()) {
                continue;
            }
        }

        if (!SyncNow(nullptr, 2000)) {
            ESP_LOGW(kTag, "Queued network time sync failed");
            Notify();
        }
    }
}

Result MakeSuccess(std::string message)
{
    Result result;
    result.success = true;
    result.status_code = 200;
    result.message = std::move(message);
    return result;
}

Result MakeError(int status_code, std::string message)
{
    Result result;
    result.success = false;
    result.status_code = status_code;
    result.message = std::move(message);
    return result;
}

Result MakeValidationError(const char* field, const char* error_code, std::string message)
{
    Result result;
    result.success = false;
    result.validation_error = true;
    result.status_code = 400;
    result.field = field != nullptr ? field : "";
    result.error_code = error_code != nullptr ? error_code : "";
    result.message = std::move(message);
    return result;
}

std::string ReadRequestBody(httpd_req_t* request)
{
    if (request == nullptr || request->content_len <= 0) {
        return {};
    }

    std::string body(static_cast<size_t>(request->content_len), '\0');
    size_t offset = 0;
    while (offset < body.size()) {
        const int received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) {
            return {};
        }
        offset += static_cast<size_t>(received);
    }
    return body;
}

std::string JsonString(cJSON* root)
{
    if (root == nullptr) {
        return "{}";
    }
    char* raw = cJSON_PrintUnformatted(root);
    if (raw == nullptr) {
        return "{}";
    }
    std::string json(raw);
    cJSON_free(raw);
    return json;
}

esp_err_t SendJsonResponse(httpd_req_t* request, int status_code, cJSON* root)
{
    if (request == nullptr) {
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        return ESP_FAIL;
    }

    const std::string payload = JsonString(root);
    if (root != nullptr) {
        cJSON_Delete(root);
    }

    switch (status_code) {
        case 200:
            httpd_resp_set_status(request, HTTPD_200);
            break;
        case 400:
            httpd_resp_set_status(request, HTTPD_400);
            break;
        default:
            httpd_resp_set_status(request, HTTPD_500);
            break;
    }
    httpd_resp_set_type(request, "application/json; charset=utf-8");
    return httpd_resp_send(request, payload.c_str(), payload.size());
}

void AppendRuntime(cJSON* runtime, const RuntimeSnapshot& snapshot)
{
    cJSON_AddBoolToObject(runtime, "clock_enabled", snapshot.clock_enabled);
    cJSON_AddBoolToObject(runtime, "time_valid", snapshot.time_valid);
    cJSON_AddStringToObject(runtime, "time_source", TimeSourceName(snapshot.time_source));
    cJSON_AddBoolToObject(runtime, "has_network_sync", snapshot.has_network_sync);
    cJSON_AddBoolToObject(runtime, "sync_in_progress", snapshot.sync_in_progress);
    cJSON_AddNumberToObject(runtime, "last_network_sync_epoch", snapshot.last_network_sync_epoch);
    cJSON_AddStringToObject(runtime, "current_date", snapshot.current_date.c_str());
    cJSON_AddStringToObject(runtime, "current_time", snapshot.current_time.c_str());
}

void AppendSnapshot(cJSON* root, const Snapshot& snapshot, const char* message)
{
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", message != nullptr ? message : "");

    cJSON* settings = cJSON_AddObjectToObject(root, "settings");
    cJSON_AddBoolToObject(settings, "enabled", snapshot.settings.enabled);
    cJSON_AddStringToObject(settings, "timezone_name", snapshot.settings.timezone_name.c_str());
    cJSON_AddStringToObject(settings, "location", snapshot.settings.location.c_str());

    cJSON* runtime = cJSON_AddObjectToObject(root, "runtime");
    AppendRuntime(runtime, snapshot.runtime);
}

bool ParsePatchBody(const std::string& body, SettingsPatch* patch, std::string* error)
{
    if (patch == nullptr) {
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
    if (root == nullptr) {
        if (error != nullptr) {
            *error = "Invalid JSON body";
        }
        return false;
    }

    cJSON* enabled = cJSON_GetObjectItemCaseSensitive(root, "enabled");
    if (cJSON_IsBool(enabled)) {
        patch->has_enabled = true;
        patch->enabled = cJSON_IsTrue(enabled);
    } else if (enabled != nullptr && !cJSON_IsNull(enabled)) {
        if (error != nullptr) {
            *error = "Invalid enabled";
        }
        cJSON_Delete(root);
        return false;
    }

    cJSON* timezone_name = cJSON_GetObjectItemCaseSensitive(root, "timezone_name");
    if (cJSON_IsString(timezone_name) && timezone_name->valuestring != nullptr) {
        patch->has_timezone_name = true;
        patch->timezone_name = timezone_name->valuestring;
    } else if (timezone_name != nullptr && !cJSON_IsNull(timezone_name)) {
        if (error != nullptr) {
            *error = "Invalid timezone_name";
        }
        cJSON_Delete(root);
        return false;
    }

    cJSON* location = cJSON_GetObjectItemCaseSensitive(root, "location");
    if (cJSON_IsString(location) && location->valuestring != nullptr) {
        patch->has_location = true;
        patch->location = location->valuestring;
    } else if (location != nullptr && !cJSON_IsNull(location)) {
        if (error != nullptr) {
            *error = "Invalid location";
        }
        cJSON_Delete(root);
        return false;
    }

    cJSON* manual_date = cJSON_GetObjectItemCaseSensitive(root, "manual_date");
    cJSON* manual_time = cJSON_GetObjectItemCaseSensitive(root, "manual_time");
    const bool has_manual_date =
        cJSON_IsString(manual_date) && manual_date->valuestring != nullptr;
    const bool has_manual_time =
        cJSON_IsString(manual_time) && manual_time->valuestring != nullptr;
    if (has_manual_date != has_manual_time) {
        if (error != nullptr) {
            *error = "manual_date and manual_time must be provided together";
        }
        cJSON_Delete(root);
        return false;
    }
    if (has_manual_date && has_manual_time) {
        patch->has_manual_datetime = true;
        patch->manual_date = manual_date->valuestring;
        patch->manual_time = manual_time->valuestring;
    }

    cJSON_Delete(root);
    return true;
}

esp_err_t RegisterPortalRoute(httpd_handle_t server, const httpd_uri_t* handler)
{
    if (server == nullptr || handler == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t err = httpd_register_uri_handler(server, handler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register timezone portal route %s [%d]: %s",
                 handler->uri != nullptr ? handler->uri : "<null>",
                 static_cast<int>(handler->method),
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t HandlePortalTimeSettings(httpd_req_t* request)
{
    cJSON* root = cJSON_CreateObject();
    AppendSnapshot(root, GetSnapshot(), "Time settings loaded");
    return SendJsonResponse(request, 200, root);
}

esp_err_t HandlePortalTimeSettingsPatch(httpd_req_t* request)
{
    if (request == nullptr ||
        request->content_len <= 0 ||
        request->content_len > static_cast<int>(kMaxPortalPayloadLen)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Invalid time settings payload");
        return SendJsonResponse(request, 400, root);
    }

    const std::string body = ReadRequestBody(request);
    SettingsPatch patch = {};
    std::string parse_error;
    if (body.empty() || !ParsePatchBody(body, &patch, &parse_error)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message",
                                parse_error.empty() ? "Invalid time settings payload"
                                                    : parse_error.c_str());
        return SendJsonResponse(request, 400, root);
    }

    const Result result = ApplySettingsPatch(patch);
    if (!result.success) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", result.message.c_str());
        if (!result.field.empty()) {
            cJSON_AddStringToObject(root, "field", result.field.c_str());
        }
        if (!result.error_code.empty()) {
            cJSON_AddStringToObject(root, "error_code", result.error_code.c_str());
        }
        return SendJsonResponse(request, result.status_code, root);
    }

    cJSON* root = cJSON_CreateObject();
    AppendSnapshot(root, GetSnapshot(), result.message.c_str());
    return SendJsonResponse(request, 200, root);
}

esp_err_t HandlePortalTimeRuntime(httpd_req_t* request)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", "Time runtime loaded");
    cJSON* runtime = cJSON_AddObjectToObject(root, "runtime");
    AppendRuntime(runtime, GetSnapshot().runtime);
    return SendJsonResponse(request, 200, root);
}

esp_err_t HandlePortalTimezoneList(httpd_req_t* request)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", "Timezone list loaded");
    cJSON* timezones = cJSON_AddArrayToObject(root, "timezones");
    for (const TimezoneInfo& timezone : ListTimezones()) {
        cJSON* item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "name", timezone.name.c_str());
        cJSON_AddStringToObject(item, "description", timezone.description.c_str());
        cJSON_AddItemToArray(timezones, item);
    }
    return SendJsonResponse(request, 200, root);
}

}  // namespace

esp_err_t Init()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_initialized) {
            return ESP_OK;
        }

        LoadSettingsFromStorage();
        LoadTimeStatusFromStorage();
        if (!s_timezone_name.empty() && !ApplyTimezoneByName(s_timezone_name)) {
            ESP_LOGW(kTag, "Configured timezone is invalid: %s", s_timezone_name.c_str());
        }
        s_initialized = true;
    }

    if (s_sync_queue == nullptr) {
        s_sync_queue = xQueueCreate(kSyncQueueDepth, sizeof(SyncRequest));
        if (s_sync_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }
    if (s_sync_task == nullptr) {
            const BaseType_t created = xTaskCreatePinnedToCore(
                SyncWorker,
                "timezone_sync",
                kSyncTaskStackWords,
                nullptr,
                followup_task_config::kPriorityTimezoneSync,
                &s_sync_task,
                followup_task_config::kSystemCore);
        if (created != pdPASS) {
            s_sync_task = nullptr;
            return ESP_ERR_NO_MEM;
        }
    }

    if (!SetSystemTimeFromRtc()) {
        ESP_LOGW(kTag, "RTC time unavailable or invalid at startup");
    }

    ESP_LOGI(kTag, "Timezone service initialized: enabled=%d timezone=%s",
             s_enabled ? 1 : 0,
             s_timezone_name.empty() ? "<unset>" : s_timezone_name.c_str());
    Notify();
    return ESP_OK;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

Snapshot GetSnapshot()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildSnapshotLocked();
}

std::vector<TimezoneInfo> ListTimezones()
{
    std::vector<TimezoneInfo> timezones;
    timezones.reserve(std::size(kTimezones));
    for (const auto& timezone : kTimezones) {
        timezones.push_back({
            .name = timezone.name,
            .description = timezone.description,
        });
    }
    std::sort(timezones.begin(), timezones.end(),
              [](const TimezoneInfo& lhs, const TimezoneInfo& rhs) {
                  const std::string& lhs_label =
                      lhs.description.empty() ? lhs.name : lhs.description;
                  const std::string& rhs_label =
                      rhs.description.empty() ? rhs.name : rhs.description;
                  return lhs_label < rhs_label;
              });
    return timezones;
}

void SetNetworkConnected(bool connected)
{
    bool should_sync = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const bool was_connected = s_network_connected;
        s_network_connected = connected;
        // Only sync on the disconnected->connected transition so repeated "connected"
        // events (RSSI updates, etc.) don't queue redundant NTP syncs.
        should_sync = connected && !was_connected && ShouldSyncOnNetworkConnectedLocked();
    }

    if (should_sync) {
        QueueSync(false);
    }
    Notify();
}

Result ApplySettingsPatch(const SettingsPatch& patch)
{
    bool enable_clock = false;
    bool use_network_time = false;
    std::string staged_timezone_name;
    std::string staged_location;
    bool timezone_checked = false;
    bool has_manual_epoch = false;
    time_t manual_epoch = 0;

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        staged_timezone_name = s_timezone_name;
        staged_location = s_location;
        if (patch.has_timezone_name) {
            staged_timezone_name = TrimCopy(patch.timezone_name);
        }
        if (patch.has_location) {
            staged_location = TrimCopy(patch.location);
        }
        enable_clock = patch.has_enabled ? patch.enabled : s_enabled;

        if (patch.has_manual_datetime && staged_timezone_name.empty()) {
            return MakeValidationError("timezone_name", "required_for_manual_time",
                                       "timezone_name required to set manual time");
        }
        if (enable_clock && staged_timezone_name.empty()) {
            return MakeValidationError("timezone_name", "required_for_clock",
                                       "timezone_name required to enable clock");
        }
        use_network_time = s_network_connected && !staged_timezone_name.empty();
    }

    if (!staged_timezone_name.empty()) {
        if (!ApplyTimezoneByName(staged_timezone_name)) {
            return MakeValidationError("timezone_name", "invalid_timezone",
                                       "Invalid timezone_name");
        }
        timezone_checked = true;
    }

    if (patch.has_manual_datetime && !use_network_time) {
        if (!timezone_checked && !ApplyTimezoneByName(staged_timezone_name)) {
            return MakeValidationError("timezone_name", "invalid_timezone",
                                       "Invalid timezone_name");
        }
        if (!ParseLocalDateTime(patch.manual_date, patch.manual_time, &manual_epoch)) {
            return MakeValidationError("manual_date", "invalid_manual_time",
                                       "Invalid manual date/time");
        }
        has_manual_epoch = true;
        if (!SetSystemEpoch(manual_epoch, TimeSource::kManual, true) ||
            !SetRtcFromEpoch(manual_epoch)) {
            return MakeError(500, "Failed to set manual time");
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_enabled = enable_clock;
        s_timezone_name = staged_timezone_name;
        s_location = staged_location;
        if (!SaveSettingsToStorageLocked()) {
            return MakeError(500, "Failed to save time settings");
        }
    }

    if (use_network_time) {
        // Queue the NTP sync on the dedicated sync worker rather than running the
        // stack-heavy, ~2s-blocking SNTP path inline. ApplySettingsPatch is called from
        // the caller's task (e.g. the 4 KB touch task when the user taps "Sync & Save"),
        // and running SNTP there overflowed its stack. The sync result arrives later via
        // the SNTP callback -> Notify -> event.
        QueueSync(true);
    } else if (enable_clock && !has_manual_epoch && !staged_timezone_name.empty()) {
        if (!timezone_checked && !ApplyTimezoneByName(staged_timezone_name)) {
            return MakeValidationError("timezone_name", "invalid_timezone",
                                       "Invalid timezone_name");
        }
    }

    Notify();
    return MakeSuccess(enable_clock ? "Time settings updated" : "Clock disabled");
}

bool SyncNow(const char* ntp_server, uint32_t timeout_ms)
{
    std::string timezone_name;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        timezone_name = s_timezone_name;
    }

    const TimezoneCatalogEntry* timezone =
        timezone_name.empty() ? nullptr : FindTimezoneByName(timezone_name.c_str());
    const char* timezone_tz = timezone != nullptr ? timezone->tz_string : getenv("TZ");
    const char* server = ntp_server != nullptr && ntp_server[0] != '\0'
                             ? ntp_server
                             : PickDefaultNtpServer(timezone_tz);

    s_sync_in_progress.store(true, std::memory_order_relaxed);
    Notify();
    ESP_LOGI(kTag, "NTP sync begin: server=%s timezone=%s",
             server,
             timezone_name.empty() ? "<env>" : timezone_name.c_str());

    s_sntp_sync_seen = false;
    esp_sntp_set_time_sync_notification_cb(OnSntpTimeSync);
    esp_sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    if (esp_sntp_enabled()) {
        esp_sntp_stop();
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, server);
    esp_sntp_init();
    if (timezone_tz != nullptr && timezone_tz[0] != '\0') {
        setenv("TZ", timezone_tz, 1);
        tzset();
    }

    const int64_t start_us = esp_timer_get_time();
    time_t now = time(nullptr);
    while ((esp_timer_get_time() - start_us) / 1000 < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(100));
        now = time(nullptr);
        if (now >= kMinValidEpoch &&
            (s_sntp_sync_seen || esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)) {
            break;
        }
    }

    bool success = false;
    if (now >= kMinValidEpoch) {
        success = SetSystemEpoch(now, TimeSource::kNtp, false);
        if (success && !SetRtcFromEpoch(now)) {
            ESP_LOGW(kTag, "System time synced, but RTC update failed");
        }
    } else {
        ESP_LOGW(kTag, "NTP sync did not produce valid time within timeout");
    }

    s_sync_in_progress.store(false, std::memory_order_relaxed);
    ESP_LOGI(kTag, "NTP sync %s", success ? "succeeded" : "failed");
    Notify();
    return success;
}

bool IsSyncInProgress()
{
    return s_sync_in_progress.load(std::memory_order_relaxed);
}

void RegisterPortalRoutes(httpd_handle_t server)
{
    if (server == nullptr) {
        return;
    }

    httpd_uri_t settings_get = {
        .uri = kPortalApiSettingsTimeUri,
        .method = HTTP_GET,
        .handler = HandlePortalTimeSettings,
        .user_ctx = nullptr,
    };
    httpd_uri_t settings_patch = {
        .uri = kPortalApiSettingsTimeUri,
        .method = HTTP_PATCH,
        .handler = HandlePortalTimeSettingsPatch,
        .user_ctx = nullptr,
    };
    httpd_uri_t runtime_get = {
        .uri = kPortalApiRuntimeTimeUri,
        .method = HTTP_GET,
        .handler = HandlePortalTimeRuntime,
        .user_ctx = nullptr,
    };
    httpd_uri_t timezone_list = {
        .uri = kPortalApiTimezoneListUri,
        .method = HTTP_GET,
        .handler = HandlePortalTimezoneList,
        .user_ctx = nullptr,
    };

    if (RegisterPortalRoute(server, &settings_get) != ESP_OK ||
        RegisterPortalRoute(server, &settings_patch) != ESP_OK ||
        RegisterPortalRoute(server, &runtime_get) != ESP_OK ||
        RegisterPortalRoute(server, &timezone_list) != ESP_OK) {
        ESP_LOGW(kTag, "Timezone portal routes are incomplete");
    }
}

const char* TimeSourceName(TimeSource source)
{
    switch (source) {
        case TimeSource::kRtc:
            return "RTC";
        case TimeSource::kManual:
            return "MANUAL";
        case TimeSource::kNtp:
            return "NETWORK";
        case TimeSource::kUnknown:
        default:
            return "UNSET";
    }
}

}  // namespace timezone_service
