#ifndef EPAPER_UI_STATUS_BAR_H_
#define EPAPER_UI_STATUS_BAR_H_

#include <cstdint>
#include <string>

namespace epaper_ui {

enum class WifiStatus : uint8_t {
    kDisconnected,
    kConnected,
    kAccessPoint,
    kDisabled,
};

struct BatteryStatus {
    int percent = -1;
    bool charging = false;
};

struct StatusBarState {
    BatteryStatus battery = {};
    WifiStatus wifi = WifiStatus::kDisabled;
    std::string time_text = {};
    bool show_gemini_icon = false;
    bool show_power_icon = false;
    bool show_sleep_icon = false;
};

int StatusBarHeight();

void DrawStatusBar(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const StatusBarState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_STATUS_BAR_H_
