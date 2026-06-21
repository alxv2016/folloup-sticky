#include "status_bar_runtime.h"

#include <mutex>

#include "esp_check.h"
#include "gemini_service.h"
#include "power_service.h"
#include "timezone_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"

namespace status_bar_runtime {
namespace {

std::mutex s_state_mutex;
bool s_sleep_indicator_visible = false;
bool s_shutdown_indicator_visible = false;

epaper_ui::WifiStatus BuildWifiStatus()
{
    const wifi_service::UiState ui_state = wifi_service::GetUiState();
    if (!ui_state.wifi_enabled) {
        return epaper_ui::WifiStatus::kDisabled;
    }
    if (ui_state.access_point_mode) {
        return epaper_ui::WifiStatus::kAccessPoint;
    }
    if (ui_state.connected) {
        return epaper_ui::WifiStatus::kConnected;
    }
    return epaper_ui::WifiStatus::kDisconnected;
}

std::string BuildTimeText()
{
    const timezone_service::Snapshot snapshot = timezone_service::GetSnapshot();
    if (!snapshot.runtime.current_time.empty()) {
        return snapshot.runtime.current_time;
    }
    return "--:--";
}

}  // namespace

void SetSleepIndicatorVisible(bool visible)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_sleep_indicator_visible = visible;
}

void SetShutdownIndicatorVisible(bool visible)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_shutdown_indicator_visible = visible;
}

epaper_ui::StatusBarState BuildState()
{
    epaper_ui::StatusBarState state = {};
    state.wifi = BuildWifiStatus();
    state.time_text = BuildTimeText();
    state.show_gemini_icon =
        state.wifi == epaper_ui::WifiStatus::kConnected &&
        gemini_service::GetSnapshot().runtime.ready;

    power_service::Status power_status = {};
    if (power_service::ReadStatus(&power_status) == ESP_OK && power_status.battery.available) {
        state.battery.percent =
            static_cast<int>(power_status.battery.state_of_charge_percent);
        state.battery.charging =
            power_status.charge_state == power_service::ChargeState::kCharging;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        state.show_sleep_icon = s_sleep_indicator_visible;
        state.show_power_icon = s_shutdown_indicator_visible;
    }

    return state;
}

esp_err_t UpdateDisplayState()
{
    return display_service::SetStatusBarState(BuildState());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kStatusBar, &UpdateDisplayState, refresh_mode);
}

esp_err_t UpdateDisplayStateAndRefreshNow(display_service::RefreshMode refresh_mode)
{
    ESP_RETURN_ON_ERROR(UpdateDisplayState(), "StatusBarRuntime", "set status bar state failed");
    return display_service::RefreshCurrentScreen(refresh_mode);
}

}  // namespace status_bar_runtime
