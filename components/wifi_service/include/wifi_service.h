#ifndef WIFI_SERVICE_H_
#define WIFI_SERVICE_H_

#include <string>
#include <vector>

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_wifi_types.h"

namespace wifi_service {

enum class State {
    kIdle,
    kScanning,
    kScanCompleted,
    kScanFailed,
    kConnecting,
    kConnected,
    kDisconnected,
    kAccessPointMode,
};

enum class ScanState : uint8_t {
    kIdle = 0,
    kRunning,
    kComplete,
    kFailed,
};

struct ScannedNetwork {
    std::string ssid;
    int rssi = 0;
    wifi_auth_mode_t auth_mode = WIFI_AUTH_OPEN;

    bool IsOpen() const { return auth_mode == WIFI_AUTH_OPEN; }
};

struct ScanSnapshot {
    ScanState state = ScanState::kIdle;
    std::vector<ScannedNetwork> networks = {};
    esp_err_t last_error = ESP_OK;
};

struct UiState {
    bool wifi_enabled = true;
    bool connected = false;
    bool access_point_mode = false;
    bool reconnecting = false;
    bool has_saved_credentials = false;
    std::string ssid;
    std::string ip_address;
    std::string ap_ssid;
    std::string ap_url;
    int rssi = 0;
};

struct Event {
    State state = State::kIdle;
    std::string detail;
    UiState ui_state = {};
};

using EventHandler = void (*)(const Event& event, void* context);
using PortalRouteRegistrar = void (*)(httpd_handle_t server, void* context);

esp_err_t Init();
void Start();
void SetEventHandler(EventHandler handler, void* context);
void SetPortalRouteRegistrar(PortalRouteRegistrar registrar, void* context);
void SetWifiEnabled(bool enabled);
void EnterAccessPointMode();
bool ConnectToNetwork(const std::string& ssid, const std::string& password,
                      bool save_on_success = true);
bool DisconnectFromNetwork(bool clear_saved_credentials = true);
bool StartNetworkScan();
bool ClearSavedCredentials();

UiState GetUiState();
ScanSnapshot GetScanSnapshot();
bool IsConnected();
bool IsAccessPointMode();
bool IsBusy();
bool HasSavedCredentials();

const char* StateName(State state);

}  // namespace wifi_service

#endif  // WIFI_SERVICE_H_
