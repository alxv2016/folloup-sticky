#include "wifi_service.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "sdkconfig.h"

namespace wifi_service {
namespace {

constexpr const char* kTag = "WifiService";
constexpr const char* kNvsNamespace = "wifi";
constexpr const char* kSsidKey = "ssid";
constexpr const char* kPasswordKey = "password";
constexpr const char* kApUrl = "http://192.168.4.1";
constexpr int kConnectTimeoutSec = 60;
constexpr size_t kMaxPortalPayloadLen = 512;
constexpr uint32_t kTransitionTaskStackWords = 8192;
constexpr UBaseType_t kTransitionQueueDepth = 4;
constexpr uint32_t kCallbackTaskStackWords = 6144;
constexpr size_t kMaxPendingCallbacks = 16;
constexpr const char* kPortalApiScanUri = "/api/scan";
constexpr const char* kPortalApiConfigureUri = "/api/configure";
constexpr const char* kPortalApiStatusUri = "/api/status";
constexpr const char* kPortalApiDisconnectUri = "/api/disconnect";

enum class TransitionRequest : uint8_t {
    kStart,
    kStartStation,
    kEnterAccessPoint,
    kDisableAccessPoint,
    kDisconnectStation,
    kStopWifi,
};

struct Credentials {
    std::string ssid;
    std::string password;

    bool valid() const { return !ssid.empty(); }
};

bool s_initialized = false;
bool s_stack_initialized = false;
bool s_wifi_enabled = true;
bool s_connected = false;
bool s_access_point_mode = false;
bool s_suppress_disconnect_event = false;
bool s_connect_timer_active = false;
bool s_reconnecting = false;
bool s_persist_active_credentials_on_success = false;
bool s_clear_saved_credentials_on_disconnect = true;
int s_rssi = 0;
std::string s_current_ssid;
std::string s_ip_address;
std::string s_ap_ssid;
std::string s_ap_url = kApUrl;
Credentials s_saved_credentials;
Credentials s_active_credentials;
ScanSnapshot s_scan_snapshot = {};
std::mutex s_state_mutex;
std::mutex s_callback_mutex;
std::deque<Event> s_pending_events;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
PortalRouteRegistrar s_portal_registrar = nullptr;
void* s_portal_registrar_context = nullptr;
QueueHandle_t s_transition_queue = nullptr;
TaskHandle_t s_transition_task = nullptr;
TaskHandle_t s_callback_task = nullptr;
esp_timer_handle_t s_connect_timer = nullptr;
esp_netif_t* s_sta_netif = nullptr;
esp_netif_t* s_ap_netif = nullptr;
httpd_handle_t s_portal_server = nullptr;
esp_event_handler_instance_t s_wifi_event_handler = nullptr;
esp_event_handler_instance_t s_ip_event_handler = nullptr;

void CheckOrAbort(esp_err_t err, const char* operation)
{
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "%s failed: %s", operation, esp_err_to_name(err));
        ESP_ERROR_CHECK(err);
    }
}

UiState BuildUiStateLocked()
{
    return UiState{
        .wifi_enabled = s_wifi_enabled,
        .connected = s_connected,
        .access_point_mode = s_access_point_mode,
        .reconnecting = s_reconnecting,
        .has_saved_credentials = s_saved_credentials.valid(),
        .ssid = s_current_ssid,
        .ip_address = s_ip_address,
        .ap_ssid = s_ap_ssid,
        .ap_url = s_ap_url,
        .rssi = s_rssi,
    };
}

void CallbackTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            Event event = {};
            EventHandler handler = nullptr;
            void* context = nullptr;
            {
                std::lock_guard<std::mutex> lock(s_callback_mutex);
                if (s_pending_events.empty()) {
                    break;
                }
                event = std::move(s_pending_events.front());
                s_pending_events.pop_front();
                handler = s_event_handler;
                context = s_event_context;
            }
            if (handler != nullptr) {
                handler(event, context);
            }
        }
    }
}

void Notify(State state, const std::string& detail = {})
{
    if (s_callback_task == nullptr) {
        return;
    }

    Event event = {};
    event.state = state;
    event.detail = detail;
    {
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        event.ui_state = BuildUiStateLocked();
    }

    {
        std::lock_guard<std::mutex> lock(s_callback_mutex);
        if (s_pending_events.size() >= kMaxPendingCallbacks) {
            s_pending_events.pop_front();
        }
        s_pending_events.push_back(std::move(event));
    }
    xTaskNotifyGive(s_callback_task);
}

std::string BuildApSsid()
{
    uint8_t mac[6] = {};
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    char ssid[64] = {};
    std::snprintf(ssid, sizeof(ssid), "%s-%02X%02X%02X",
                  CONFIG_FOLLOWUP_WIFI_AP_PREFIX,
                  mac[3],
                  mac[4],
                  mac[5]);
    return ssid;
}

std::string IpInfoToUrl(esp_netif_t* netif)
{
    if (netif == nullptr) {
        return kApUrl;
    }

    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return kApUrl;
    }

    char url[32] = {};
    std::snprintf(url, sizeof(url), "http://%u.%u.%u.%u", IP2STR(&ip_info.ip));
    return url;
}

std::string DisconnectReasonToString(uint8_t reason)
{
    switch (static_cast<wifi_err_reason_t>(reason)) {
        case WIFI_REASON_AUTH_FAIL:
            return "AUTH FAILED";
        case WIFI_REASON_NO_AP_FOUND:
            return "AP NOT FOUND";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "HANDSHAKE TIMEOUT";
        case WIFI_REASON_ASSOC_FAIL:
            return "ASSOC FAILED";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "BEACON TIMEOUT";
        default:
            break;
    }

    char detail[24] = {};
    std::snprintf(detail, sizeof(detail), "REASON %d", reason);
    return detail;
}

void ConfigureAccessPointConfig(const std::string& ap_ssid, wifi_config_t* config)
{
    if (config == nullptr) {
        return;
    }

    *config = {};
    strlcpy(reinterpret_cast<char*>(config->ap.ssid), ap_ssid.c_str(), sizeof(config->ap.ssid));
    config->ap.ssid_len = ap_ssid.size();
    config->ap.channel = 1;
    config->ap.max_connection = 4;
    config->ap.authmode = WIFI_AUTH_OPEN;
    config->ap.pmf_cfg.required = false;
}

void ConfigureStationConfig(const Credentials& credentials, wifi_config_t* config)
{
    if (config == nullptr) {
        return;
    }

    *config = {};
    strlcpy(reinterpret_cast<char*>(config->sta.ssid), credentials.ssid.c_str(),
            sizeof(config->sta.ssid));
    strlcpy(reinterpret_cast<char*>(config->sta.password), credentials.password.c_str(),
            sizeof(config->sta.password));
    config->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config->sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    config->sta.failure_retry_cnt = 0;
    config->sta.pmf_cfg.capable = true;
    config->sta.pmf_cfg.required = false;
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

void ReloadSavedCredentials()
{
    Credentials credentials = {};
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        LoadString(handle, kSsidKey, &credentials.ssid);
        LoadString(handle, kPasswordKey, &credentials.password);
        nvs_close(handle);
    }

    if (!credentials.valid()) {
        credentials.ssid = CONFIG_FOLLOWUP_WIFI_STA_SSID;
        credentials.password = CONFIG_FOLLOWUP_WIFI_STA_PASSWORD;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_saved_credentials = std::move(credentials);
    if (!s_active_credentials.valid()) {
        s_active_credentials = s_saved_credentials;
    }
}

bool SaveCredentials(const std::string& ssid, const std::string& password)
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS for Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_str(handle, kSsidKey, ssid.c_str());
    if (err == ESP_OK) {
        err = nvs_set_str(handle, kPasswordKey, password.c_str());
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_saved_credentials = Credentials{.ssid = ssid, .password = password};
    return true;
}

bool ClearCredentialsFromNvs()
{
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to open NVS to clear Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_erase_key(handle, kSsidKey);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        esp_err_t password_err = nvs_erase_key(handle, kPasswordKey);
        if (password_err != ESP_OK && password_err != ESP_ERR_NVS_NOT_FOUND) {
            err = password_err;
        }
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to clear Wi-Fi credentials: %s", esp_err_to_name(err));
        return false;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_saved_credentials = {};
    return true;
}

Credentials ResolveStationCredentialsLocked()
{
    return s_active_credentials.valid() ? s_active_credentials : s_saved_credentials;
}

void UpdateAccessPointIdentity()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_ap_ssid = BuildApSsid();
    s_ap_url = kApUrl;
}

void StopConfigPortal()
{
    if (s_portal_server == nullptr) {
        return;
    }
    httpd_stop(s_portal_server);
    s_portal_server = nullptr;
}

std::string UrlDecode(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '+') {
            decoded.push_back(' ');
            continue;
        }
        if (value[i] == '%' && i + 2 < value.size()) {
            char hex[3] = {value[i + 1], value[i + 2], '\0'};
            char* end = nullptr;
            long parsed = std::strtol(hex, &end, 16);
            if (end != nullptr && *end == '\0') {
                decoded.push_back(static_cast<char>(parsed));
                i += 2;
                continue;
            }
        }
        decoded.push_back(value[i]);
    }
    return decoded;
}

const char* AuthModeToString(wifi_auth_mode_t auth_mode)
{
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3-PSK";
        default:
            return "UNKNOWN";
    }
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

cJSON* BuildStatusJson(const UiState& ui_state, const ScanSnapshot* scan_snapshot,
                       bool include_networks, const char* message, bool success)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", success);
    cJSON_AddStringToObject(root, "message", message != nullptr ? message : "");
    cJSON_AddBoolToObject(root, "wifi_enabled", ui_state.wifi_enabled);
    cJSON_AddBoolToObject(root, "connected", ui_state.connected);
    cJSON_AddBoolToObject(root, "access_point_mode", ui_state.access_point_mode);
    cJSON_AddBoolToObject(root, "has_saved_credentials", ui_state.has_saved_credentials);
    cJSON_AddStringToObject(root, "ssid", ui_state.ssid.c_str());
    cJSON_AddNumberToObject(root, "rssi", ui_state.rssi);
    cJSON_AddStringToObject(root, "ip_address", ui_state.ip_address.c_str());
    cJSON_AddStringToObject(root, "ap_ssid", ui_state.ap_ssid.c_str());
    cJSON_AddStringToObject(root, "ap_url", ui_state.ap_url.c_str());
    if (scan_snapshot != nullptr) {
        cJSON_AddBoolToObject(root, "scan_in_progress",
                              scan_snapshot->state == ScanState::kRunning);
    }

    if (include_networks) {
        cJSON* networks = cJSON_AddArrayToObject(root, "networks");
        if (scan_snapshot != nullptr) {
            for (const ScannedNetwork& network : scan_snapshot->networks) {
                cJSON* item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "ssid", network.ssid.c_str());
                cJSON_AddNumberToObject(item, "rssi", network.rssi);
                cJSON_AddNumberToObject(item, "encryption_type",
                                        static_cast<int>(network.auth_mode));
                cJSON_AddBoolToObject(item, "is_open", network.IsOpen());
                cJSON_AddStringToObject(item, "security", AuthModeToString(network.auth_mode));
                cJSON_AddItemToArray(networks, item);
            }
        }
    }

    return root;
}

esp_err_t RegisterRoute(httpd_handle_t server, const httpd_uri_t* handler)
{
    if (server == nullptr || handler == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t err = httpd_register_uri_handler(server, handler);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to register Wi-Fi route %s [%d]: %s",
                 handler->uri != nullptr ? handler->uri : "<null>",
                 static_cast<int>(handler->method),
                 esp_err_to_name(err));
    }
    return err;
}

esp_err_t HandlePortalRoot(httpd_req_t* request)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    cJSON_AddStringToObject(root, "message", "Folloup Wi-Fi backend");
    cJSON* endpoints = cJSON_AddArrayToObject(root, "endpoints");
    cJSON_AddItemToArray(endpoints, cJSON_CreateString(kPortalApiStatusUri));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString(kPortalApiScanUri));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString(kPortalApiConfigureUri));
    cJSON_AddItemToArray(endpoints, cJSON_CreateString(kPortalApiDisconnectUri));
    return SendJsonResponse(request, 200, root);
}

esp_err_t HandlePortalStatus(httpd_req_t* request)
{
    const UiState ui_state = GetUiState();
    const ScanSnapshot snapshot = GetScanSnapshot();
    return SendJsonResponse(request, 200,
                            BuildStatusJson(ui_state, &snapshot, true,
                                            ui_state.connected ? "Connected" : "Not connected",
                                            true));
}

esp_err_t HandlePortalScan(httpd_req_t* request)
{
    const UiState ui_state = GetUiState();
    const ScanSnapshot snapshot = GetScanSnapshot();
    if (snapshot.state == ScanState::kRunning) {
        return SendJsonResponse(request, 200,
                                BuildStatusJson(ui_state, &snapshot, true,
                                                "Scanning for networks", true));
    }
    if (snapshot.state == ScanState::kComplete) {
        return SendJsonResponse(request, 200,
                                BuildStatusJson(ui_state, &snapshot, true,
                                                "Network scan complete", true));
    }
    if (!StartNetworkScan()) {
        return SendJsonResponse(request, 500,
                                BuildStatusJson(ui_state, nullptr, true,
                                                "Scan failed", false));
    }
    const UiState running_ui_state = GetUiState();
    const ScanSnapshot running_snapshot = GetScanSnapshot();
    return SendJsonResponse(request, 200,
                            BuildStatusJson(running_ui_state, &running_snapshot, true,
                                            "Scanning for networks", true));
}

esp_err_t HandlePortalConfigure(httpd_req_t* request)
{
    if (request == nullptr ||
        request->content_len <= 0 ||
        request->content_len > static_cast<int>(kMaxPortalPayloadLen)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Invalid Wi-Fi configuration payload");
        return SendJsonResponse(request, 400, root);
    }

    const std::string body = ReadRequestBody(request);
    std::string ssid;
    std::string password;
    if (!body.empty() && body.front() == '{') {
        cJSON* root = cJSON_ParseWithLength(body.c_str(), body.size());
        if (root == nullptr) {
            cJSON* error = cJSON_CreateObject();
            cJSON_AddBoolToObject(error, "success", false);
            cJSON_AddStringToObject(error, "message", "Invalid JSON body");
            return SendJsonResponse(request, 400, error);
        }
        cJSON* ssid_item = cJSON_GetObjectItemCaseSensitive(root, "ssid");
        cJSON* password_item = cJSON_GetObjectItemCaseSensitive(root, "password");
        if (cJSON_IsString(ssid_item) && ssid_item->valuestring != nullptr) {
            ssid = ssid_item->valuestring;
        }
        if (cJSON_IsString(password_item) && password_item->valuestring != nullptr) {
            password = password_item->valuestring;
        }
        cJSON_Delete(root);
    } else {
        char ssid_buffer[65] = {};
        char password_buffer[65] = {};
        if (httpd_query_key_value(body.c_str(), "ssid", ssid_buffer, sizeof(ssid_buffer)) ==
            ESP_OK) {
            ssid = UrlDecode(ssid_buffer);
        }
        httpd_query_key_value(body.c_str(), "password", password_buffer,
                              sizeof(password_buffer));
        password = UrlDecode(password_buffer);
    }

    if (ssid.empty()) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "SSID required");
        return SendJsonResponse(request, 400, root);
    }

    if (!ConnectToNetwork(ssid, password, true)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Failed to start Wi-Fi connection");
        return SendJsonResponse(request, 500, root);
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    const std::string message = "Connecting to " + ssid;
    cJSON_AddStringToObject(root, "message", message.c_str());
    cJSON_AddStringToObject(root, "ssid", ssid.c_str());
    return SendJsonResponse(request, 200, root);
}

esp_err_t HandlePortalDisconnect(httpd_req_t* request)
{
    const bool was_connected = IsConnected();
    const UiState previous = GetUiState();
    if (!DisconnectFromNetwork(true)) {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "message", "Failed to disconnect Wi-Fi");
        return SendJsonResponse(request, 500, root);
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "success", true);
    const std::string message =
        was_connected ? "Disconnected and cleared credentials for " + previous.ssid
                      : "Cleared saved Wi-Fi credentials";
    cJSON_AddStringToObject(root, "message", message.c_str());
    return SendJsonResponse(request, 200, root);
}

void StartConfigPortal()
{
    if (s_portal_server != nullptr) {
        return;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.lru_purge_enable = true;

    esp_err_t err = httpd_start(&s_portal_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Failed to start Wi-Fi backend: %s", esp_err_to_name(err));
        s_portal_server = nullptr;
        return;
    }

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = HandlePortalRoot,
        .user_ctx = nullptr,
    };
    httpd_uri_t scan = {
        .uri = kPortalApiScanUri,
        .method = HTTP_GET,
        .handler = HandlePortalScan,
        .user_ctx = nullptr,
    };
    httpd_uri_t configure = {
        .uri = kPortalApiConfigureUri,
        .method = HTTP_POST,
        .handler = HandlePortalConfigure,
        .user_ctx = nullptr,
    };
    httpd_uri_t status = {
        .uri = kPortalApiStatusUri,
        .method = HTTP_GET,
        .handler = HandlePortalStatus,
        .user_ctx = nullptr,
    };
    httpd_uri_t disconnect = {
        .uri = kPortalApiDisconnectUri,
        .method = HTTP_POST,
        .handler = HandlePortalDisconnect,
        .user_ctx = nullptr,
    };

    if (RegisterRoute(s_portal_server, &root) != ESP_OK ||
        RegisterRoute(s_portal_server, &scan) != ESP_OK ||
        RegisterRoute(s_portal_server, &configure) != ESP_OK ||
        RegisterRoute(s_portal_server, &status) != ESP_OK ||
        RegisterRoute(s_portal_server, &disconnect) != ESP_OK) {
        StopConfigPortal();
        return;
    }

    PortalRouteRegistrar registrar = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        registrar = s_portal_registrar;
        context = s_portal_registrar_context;
    }
    if (registrar != nullptr) {
        registrar(s_portal_server, context);
    }

    ESP_LOGI(kTag, "Wi-Fi backend active at %s", GetUiState().ap_url.c_str());
}

void HandleWifiEvent(int32_t event_id, void* event_data);
void HandleIpEvent(int32_t event_id, void* event_data);
void StartStationAttempt(bool allow_ap_fallback);
void TransitionWorker(void*);

void OnWifiEvent(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)base;
    HandleWifiEvent(event_id, event_data);
}

void OnIpEvent(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    (void)arg;
    (void)base;
    HandleIpEvent(event_id, event_data);
}

void OnWifiConnectTimeout(void* arg)
{
    (void)arg;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_connect_timer_active = false;
        s_reconnecting = false;
        s_connected = false;
        s_ip_address.clear();
        s_rssi = 0;
    }

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED &&
        err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(kTag, "esp_wifi_disconnect after timeout failed: %s", esp_err_to_name(err));
    }

    ESP_LOGW(kTag, "Wi-Fi connect timeout");
    Notify(State::kDisconnected, "CONNECT TIMEOUT");
}

void InitializeStack()
{
    if (s_stack_initialized) {
        return;
    }

    CheckOrAbort(esp_netif_init(), "esp_netif_init");
    CheckOrAbort(esp_event_loop_create_default(), "esp_event_loop_create_default");

    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        OnWifiEvent, nullptr,
                                                        &s_wifi_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        OnIpEvent, nullptr,
                                                        &s_ip_event_handler));
    UpdateAccessPointIdentity();
    s_stack_initialized = true;
}

bool QueueTransition(TransitionRequest request)
{
    if (s_transition_queue == nullptr) {
        return false;
    }
    if (xQueueSend(s_transition_queue, &request, 0) != pdPASS) {
        ESP_LOGW(kTag, "Wi-Fi transition queue full");
        return false;
    }
    return true;
}

void StopWifiNow()
{
    CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");
    StopConfigPortal();
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = true;
        s_connect_timer_active = false;
        s_reconnecting = false;
        s_connected = false;
        s_access_point_mode = false;
        s_current_ssid.clear();
        s_ip_address.clear();
        s_rssi = 0;
    }

    if (s_stack_initialized) {
        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_ERROR_CHECK(err);
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = false;
    }
    Notify(State::kIdle);
}

void EnterAccessPointModeNow()
{
    InitializeStack();
    UpdateAccessPointIdentity();
    CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");

    if (!GetUiState().wifi_enabled) {
        StopWifiNow();
        return;
    }

    ReloadSavedCredentials();

    Credentials credentials;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        credentials = ResolveStationCredentialsLocked();
    }
    if (credentials.valid()) {
        StartStationAttempt(false);
        return;
    }

    std::string ap_ssid;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = true;
        s_connect_timer_active = false;
        s_reconnecting = false;
        s_connected = false;
        s_access_point_mode = true;
        s_current_ssid.clear();
        s_ip_address.clear();
        s_rssi = 0;
        ap_ssid = s_ap_ssid;
    }

    wifi_config_t config = {};
    ConfigureAccessPointConfig(ap_ssid, &config);

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = false;
        s_ap_url = IpInfoToUrl(s_ap_netif);
    }

    StartConfigPortal();
    Notify(State::kAccessPointMode, GetUiState().ap_ssid);
}

void StartStationAttempt(bool allow_ap_fallback)
{
    InitializeStack();
    ReloadSavedCredentials();

    if (!GetUiState().wifi_enabled) {
        StopWifiNow();
        return;
    }

    Credentials credentials;
    bool access_point_mode = false;
    std::string ap_ssid;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        credentials = ResolveStationCredentialsLocked();
        access_point_mode = s_access_point_mode;
        ap_ssid = s_ap_ssid;
    }
    if (!credentials.valid()) {
        if (allow_ap_fallback) {
            ESP_LOGI(kTag, "No station credentials; entering AP setup mode");
            EnterAccessPointModeNow();
            return;
        }

        if (!access_point_mode) {
            StopConfigPortal();
        }
        CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");
        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            s_suppress_disconnect_event = true;
            s_connect_timer_active = false;
            s_reconnecting = false;
            s_connected = false;
            s_current_ssid.clear();
            s_ip_address.clear();
            s_rssi = 0;
        }

        esp_err_t err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_ERROR_CHECK(err);
        }
        ESP_ERROR_CHECK(esp_wifi_set_mode(access_point_mode ? WIFI_MODE_AP : WIFI_MODE_STA));
        if (access_point_mode) {
            wifi_config_t ap_config = {};
            ConfigureAccessPointConfig(ap_ssid, &ap_config);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        }
        ESP_ERROR_CHECK(esp_wifi_start());

        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            s_suppress_disconnect_event = false;
            if (access_point_mode) {
                s_ap_url = IpInfoToUrl(s_ap_netif);
            }
        }

        if (access_point_mode) {
            StartConfigPortal();
            Notify(State::kAccessPointMode, ap_ssid);
        } else {
            Notify(State::kDisconnected, "NO_CREDENTIALS");
        }
        return;
    }

    if (!access_point_mode) {
        StopConfigPortal();
    }
    CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = true;
        s_connect_timer_active = false;
        s_reconnecting = false;
        s_connected = false;
        s_active_credentials = credentials;
        s_current_ssid = credentials.ssid;
        s_ip_address.clear();
        s_rssi = 0;
    }

    wifi_config_t station_config = {};
    ConfigureStationConfig(credentials, &station_config);
    wifi_config_t ap_config = {};
    if (access_point_mode) {
        ConfigureAccessPointConfig(ap_ssid, &ap_config);
    }

    esp_err_t err = esp_wifi_stop();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_ERROR_CHECK(err);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(access_point_mode ? WIFI_MODE_APSTA : WIFI_MODE_STA));
    if (access_point_mode) {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    }
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = false;
        s_connect_timer_active = true;
        if (access_point_mode) {
            s_ap_url = IpInfoToUrl(s_ap_netif);
        }
    }

    if (access_point_mode) {
        StartConfigPortal();
    }
    Notify(State::kConnecting, credentials.ssid);
    ESP_ERROR_CHECK(esp_timer_start_once(s_connect_timer, kConnectTimeoutSec * 1000000ULL));
    ESP_ERROR_CHECK(esp_wifi_connect());
}

void DisconnectStationNow(bool clear_saved_credentials)
{
    CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_suppress_disconnect_event = true;
        s_connect_timer_active = false;
        s_reconnecting = false;
    }

    esp_err_t err = esp_wifi_disconnect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED &&
        err != ESP_ERR_WIFI_CONN) {
        ESP_LOGW(kTag, "esp_wifi_disconnect failed: %s", esp_err_to_name(err));
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_connected = false;
        s_current_ssid.clear();
        s_ip_address.clear();
        s_rssi = 0;
        s_active_credentials = {};
        s_persist_active_credentials_on_success = false;
        s_suppress_disconnect_event = false;
    }

    if (clear_saved_credentials) {
        ClearCredentialsFromNvs();
    }
    Notify(State::kDisconnected, clear_saved_credentials ? "DISCONNECTED" : "DISCONNECTED_TEMP");
}

void HandleTransitionRequest(TransitionRequest request)
{
    switch (request) {
        case TransitionRequest::kStart:
#if CONFIG_FOLLOWUP_WIFI_START_IN_AP_MODE
            EnterAccessPointModeNow();
#else
            StartStationAttempt(true);
#endif
            break;
        case TransitionRequest::kStartStation:
            StartStationAttempt(false);
            break;
        case TransitionRequest::kEnterAccessPoint:
            EnterAccessPointModeNow();
            break;
        case TransitionRequest::kDisableAccessPoint:
            StartStationAttempt(false);
            break;
        case TransitionRequest::kDisconnectStation:
            DisconnectStationNow(s_clear_saved_credentials_on_disconnect);
            break;
        case TransitionRequest::kStopWifi:
            StopWifiNow();
            break;
    }
}

void TransitionWorker(void*)
{
    TransitionRequest request = TransitionRequest::kStopWifi;
    while (true) {
        if (xQueueReceive(s_transition_queue, &request, portMAX_DELAY) == pdTRUE) {
            HandleTransitionRequest(request);
        }
    }
}

void HandleScanDoneEvent(void* event_data)
{
    auto* event = static_cast<wifi_event_sta_scan_done_t*>(event_data);
    uint16_t ap_count = 0;
    esp_err_t status = esp_wifi_scan_get_ap_num(&ap_count);
    if (status != ESP_OK) {
        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            s_scan_snapshot.state = ScanState::kFailed;
            s_scan_snapshot.last_error = status;
            s_scan_snapshot.networks.clear();
        }
        Notify(State::kScanFailed, "SCAN_FAILED");
        return;
    }

    std::vector<wifi_ap_record_t> raw_records(ap_count);
    if (ap_count > 0) {
        uint16_t count_to_copy = ap_count;
        status = esp_wifi_scan_get_ap_records(&count_to_copy, raw_records.data());
        if (status != ESP_OK) {
            {
                std::lock_guard<std::mutex> lock(s_state_mutex);
                s_scan_snapshot.state = ScanState::kFailed;
                s_scan_snapshot.last_error = status;
                s_scan_snapshot.networks.clear();
            }
            Notify(State::kScanFailed, "SCAN_FAILED");
            return;
        }
        ap_count = count_to_copy;
    }

    std::vector<ScannedNetwork> networks;
    networks.reserve(ap_count);
    for (uint16_t index = 0; index < ap_count; ++index) {
        const wifi_ap_record_t& record = raw_records[index];
        if (record.ssid[0] == '\0') {
            continue;
        }
        networks.push_back({
            .ssid = std::string(reinterpret_cast<const char*>(record.ssid)),
            .rssi = record.rssi,
            .auth_mode = record.authmode,
        });
    }

    const bool success = event != nullptr && event->status == 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_scan_snapshot.state = success ? ScanState::kComplete : ScanState::kFailed;
        s_scan_snapshot.last_error = success ? ESP_OK : ESP_FAIL;
        s_scan_snapshot.networks = std::move(networks);
    }
    Notify(success ? State::kScanCompleted : State::kScanFailed,
           success ? "NETWORK_SCAN_COMPLETE" : "SCAN_FAILED");
}

void HandleWifiEvent(int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_SCAN_DONE) {
        HandleScanDoneEvent(event_data);
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* event = static_cast<wifi_event_sta_disconnected_t*>(event_data);
        bool suppress = false;
        bool should_reconnect = false;
        std::string reconnect_ssid;
        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            suppress = s_suppress_disconnect_event;
            if (!suppress) {
                s_connected = false;
                s_ip_address.clear();
                s_rssi = 0;
                const Credentials credentials = ResolveStationCredentialsLocked();
                should_reconnect = s_wifi_enabled && credentials.valid() && !s_reconnecting;
                s_reconnecting = should_reconnect;
                reconnect_ssid = credentials.ssid;
            }
        }

        if (!suppress) {
            Notify(State::kDisconnected,
                   event != nullptr ? DisconnectReasonToString(event->reason) : "DISCONNECTED");
            if (should_reconnect) {
                ESP_LOGI(kTag,
                         "Wi-Fi disconnected; scheduling reconnect to ssid=%s",
                         reconnect_ssid.empty() ? "<unknown>" : reconnect_ssid.c_str());
                if (!QueueTransition(TransitionRequest::kStartStation)) {
                    std::lock_guard<std::mutex> lock(s_state_mutex);
                    s_reconnecting = false;
                    ESP_LOGW(kTag, "Wi-Fi reconnect queue request failed");
                }
            }
        }
    }
}

void HandleIpEvent(int32_t event_id, void* event_data)
{
    if (event_id != IP_EVENT_STA_GOT_IP || event_data == nullptr) {
        return;
    }

    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    char ip_address[32] = {};
    std::snprintf(ip_address, sizeof(ip_address), IPSTR, IP2STR(&event->ip_info.ip));

    int rssi = 0;
    wifi_ap_record_t ap_info = {};
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }

    CheckOrAbort(esp_timer_stop(s_connect_timer), "esp_timer_stop");
    Credentials credentials_to_persist = {};
    bool persist_credentials = false;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_connect_timer_active = false;
        s_reconnecting = false;
        s_connected = true;
        s_ip_address = ip_address;
        s_rssi = rssi;
        if (s_persist_active_credentials_on_success && s_active_credentials.valid()) {
            credentials_to_persist = s_active_credentials;
            persist_credentials = true;
            s_persist_active_credentials_on_success = false;
        }
    }

    if (persist_credentials) {
        if (!SaveCredentials(credentials_to_persist.ssid, credentials_to_persist.password)) {
            ESP_LOGW(kTag, "Connected but failed to persist Wi-Fi credentials");
        }
    }

    Notify(State::kConnected, ip_address);
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_timer_create_args_t timer_args = {
        .callback = OnWifiConnectTimeout,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifi_connect_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_connect_timer));

    s_transition_queue = xQueueCreate(kTransitionQueueDepth, sizeof(TransitionRequest));
    if (s_transition_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(TransitionWorker,
                                "wifi_transition",
                                kTransitionTaskStackWords,
                                nullptr,
                                followup_task_config::kPriorityWifiTransition,
                                &s_transition_task,
                                followup_task_config::kSystemCore) != pdPASS) {
        s_transition_task = nullptr;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(CallbackTask,
                                "wifi_callbacks",
                                kCallbackTaskStackWords,
                                nullptr,
                                followup_task_config::kPriorityWifiCallbacks,
                                &s_callback_task,
                                followup_task_config::kSystemCore) != pdPASS) {
        s_callback_task = nullptr;
        return ESP_ERR_NO_MEM;
    }

    ReloadSavedCredentials();
    s_initialized = true;
    ESP_LOGI(kTag, "Wi-Fi service initialized");
    return ESP_OK;
}

void Start()
{
    if (!s_initialized && Init() != ESP_OK) {
        return;
    }
    QueueTransition(TransitionRequest::kStart);
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_callback_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

void SetPortalRouteRegistrar(PortalRouteRegistrar registrar, void* context)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_portal_registrar = registrar;
    s_portal_registrar_context = context;
}

void SetWifiEnabled(bool enabled)
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_wifi_enabled = enabled;
    }
    QueueTransition(enabled ? TransitionRequest::kStart : TransitionRequest::kStopWifi);
}

void SetAccessPointEnabled(bool enabled)
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_access_point_mode = enabled;
        if (enabled) {
            s_wifi_enabled = true;
        }
    }
    QueueTransition(enabled ? TransitionRequest::kEnterAccessPoint
                            : TransitionRequest::kDisableAccessPoint);
}

void EnterAccessPointMode()
{
    SetAccessPointEnabled(true);
}

bool ConnectToNetwork(const std::string& ssid, const std::string& password, bool save_on_success)
{
    if (ssid.empty() || ssid.size() >= 65 || password.size() >= 65) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_wifi_enabled = true;
        s_active_credentials = {.ssid = ssid, .password = password};
        s_persist_active_credentials_on_success = save_on_success;
        s_current_ssid = ssid;
    }

    return QueueTransition(TransitionRequest::kStartStation);
}

bool DisconnectFromNetwork(bool clear_saved_credentials)
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_clear_saved_credentials_on_disconnect = clear_saved_credentials;
    }
    return QueueTransition(TransitionRequest::kDisconnectStation);
}

bool StartNetworkScan()
{
    InitializeStack();
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_wifi_enabled || s_scan_snapshot.state == ScanState::kRunning) {
            return s_wifi_enabled;
        }
    }

    const UiState state = GetUiState();
    const wifi_mode_t desired_mode = state.access_point_mode ? WIFI_MODE_APSTA : WIFI_MODE_STA;
    wifi_mode_t current_mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&current_mode);
    if (err != ESP_OK) {
        return false;
    }

    if (current_mode != desired_mode) {
        err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
            return false;
        }
        err = esp_wifi_set_mode(desired_mode);
        if (err != ESP_OK) {
            return false;
        }
        if (desired_mode == WIFI_MODE_APSTA) {
            wifi_config_t ap_config = {};
            strlcpy(reinterpret_cast<char*>(ap_config.ap.ssid), state.ap_ssid.c_str(),
                    sizeof(ap_config.ap.ssid));
            ap_config.ap.ssid_len = state.ap_ssid.size();
            ap_config.ap.channel = 1;
            ap_config.ap.max_connection = 4;
            ap_config.ap.authmode = WIFI_AUTH_OPEN;
            err = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
            if (err != ESP_OK) {
                return false;
            }
        }
    }

    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return false;
    }

    wifi_scan_config_t scan_config = {};
    scan_config.ssid = nullptr;
    scan_config.bssid = nullptr;
    scan_config.channel = 0;
    scan_config.show_hidden = false;
    scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    scan_config.scan_time.active.min = 30;
    scan_config.scan_time.active.max = 80;
    scan_config.home_chan_dwell_time = 30;
    err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            s_scan_snapshot.state = ScanState::kFailed;
            s_scan_snapshot.last_error = err;
            s_scan_snapshot.networks.clear();
        }
        Notify(State::kScanFailed, "SCAN_FAILED");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_scan_snapshot.state = ScanState::kRunning;
        s_scan_snapshot.last_error = ESP_OK;
        s_scan_snapshot.networks.clear();
    }
    Notify(State::kScanning, "NETWORK_SCAN");
    return true;
}

bool ClearSavedCredentials()
{
    return ClearCredentialsFromNvs();
}

void RecoverAfterLightSleep()
{
    if (!s_initialized) {
        return;
    }

    bool wifi_enabled = false;
    bool connected = false;
    bool access_point_mode = false;
    bool reconnecting = false;
    bool has_credentials = false;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        wifi_enabled = s_wifi_enabled;
        connected = s_connected;
        access_point_mode = s_access_point_mode;
        reconnecting = s_reconnecting;
        has_credentials = ResolveStationCredentialsLocked().valid();
    }

    if (!wifi_enabled) {
        ESP_LOGI(kTag, "Skipping Wi-Fi recovery after light sleep; Wi-Fi disabled");
        return;
    }

    if (access_point_mode) {
        wifi_mode_t current_mode = WIFI_MODE_NULL;
        const esp_err_t mode_err = esp_wifi_get_mode(&current_mode);
        if (mode_err == ESP_OK &&
            (current_mode == WIFI_MODE_AP || current_mode == WIFI_MODE_APSTA)) {
            ESP_LOGI(kTag, "Wi-Fi AP mode still active after light sleep");
            return;
        }

        ESP_LOGI(kTag, "Recovering Wi-Fi AP mode after light sleep");
        (void)QueueTransition(TransitionRequest::kEnterAccessPoint);
        return;
    }

    bool link_healthy = connected;
    if (connected) {
        wifi_ap_record_t ap_info = {};
        const esp_err_t ap_err = esp_wifi_sta_get_ap_info(&ap_info);
        link_healthy = ap_err == ESP_OK;
        if (!link_healthy) {
            ESP_LOGW(kTag,
                     "Wi-Fi link check after light sleep failed: %s",
                     esp_err_to_name(ap_err));
        }
    }

    if ((connected && link_healthy) || reconnecting || !has_credentials) {
        ESP_LOGI(kTag,
                 "Wi-Fi recovery after light sleep not needed: connected=%d link_healthy=%d reconnecting=%d has_credentials=%d",
                 connected ? 1 : 0,
                 link_healthy ? 1 : 0,
                 reconnecting ? 1 : 0,
                 has_credentials ? 1 : 0);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_connected = false;
        s_reconnecting = true;
        s_ip_address.clear();
        s_rssi = 0;
    }

    ESP_LOGI(kTag, "Recovering Wi-Fi station after light sleep");
    if (!QueueTransition(TransitionRequest::kStartStation)) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_reconnecting = false;
        ESP_LOGW(kTag, "Wi-Fi recovery queue request failed after light sleep");
    }
}

UiState GetUiState()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return BuildUiStateLocked();
}

ScanSnapshot GetScanSnapshot()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_scan_snapshot;
}

bool IsConnected()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_connected;
}

bool IsAccessPointMode()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_access_point_mode;
}

bool IsBusy()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_connect_timer_active || s_reconnecting || s_scan_snapshot.state == ScanState::kRunning;
}

bool HasSavedCredentials()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_saved_credentials.valid();
}

const char* StateName(State state)
{
    switch (state) {
        case State::kIdle:
            return "idle";
        case State::kScanning:
            return "scanning";
        case State::kScanCompleted:
            return "scan_completed";
        case State::kScanFailed:
            return "scan_failed";
        case State::kConnecting:
            return "connecting";
        case State::kConnected:
            return "connected";
        case State::kDisconnected:
            return "disconnected";
        case State::kAccessPointMode:
            return "access_point";
        default:
            return "unknown";
    }
}

}  // namespace wifi_service
