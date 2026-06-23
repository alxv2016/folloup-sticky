#ifndef UI_REFRESH_RUNTIME_H_
#define UI_REFRESH_RUNTIME_H_

#include "display_service.h"
#include "esp_err.h"

namespace ui_refresh_runtime {

enum class SurfaceKey {
    kOverlay,
    kLockScreen,
    kStatusBar,
    kFooter,
    kSettingsPage,
    kWifiPage,
};

using ApplyCallback = esp_err_t (*)();

esp_err_t Init();
bool IsInitialized();
esp_err_t Schedule(SurfaceKey key,
                   ApplyCallback apply_callback,
                   display_service::RefreshMode refresh_mode =
                       display_service::RefreshMode::kPartial);
esp_err_t ScheduleOverlay(
    SurfaceKey key,
    ApplyCallback apply_callback,
    display_service::OverlayRefreshPolicy policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay);
esp_err_t RequestRefresh(SurfaceKey key,
                         display_service::RefreshMode refresh_mode =
                             display_service::RefreshMode::kPartial);

}  // namespace ui_refresh_runtime

#endif  // UI_REFRESH_RUNTIME_H_
