#ifndef DISPLAY_SERVICE_H_
#define DISPLAY_SERVICE_H_

#include "epaper_ui/lock_screen.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/keyboard.h"
#include "epaper_ui/settings_page.h"
#include "epaper_ui/shutdown_modal.h"
#include "epaper_ui/storage_modal.h"
#include "epaper_ui/select_modal.h"
#include "epaper_ui/dashboard_page.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/time_page.h"
#include "epaper_ui/toast.h"
#include "epaper_ui/wifi_page.h"
#include "esp_err.h"

namespace display_service {

enum class ScreenId {
    kHome,
    kSettings,
    kWifi,
    kTime,
    kLockScreen,
};

enum class RefreshMode {
    kPartial,
    kFull,
};

enum class RefreshScope {
    kScreen,
    kRegion,
};

struct RefreshRequest {
    RefreshMode refresh_mode = RefreshMode::kPartial;
    RefreshScope scope = RefreshScope::kScreen;
};

enum class OverlayRefreshPolicy {
    // Rebuild the underlay from the current screen with a partial refresh. Used
    // when an overlay appears or moves (the overlay is drawn on top, so partial
    // is fine and avoids a full-screen flash on every popup).
    kRebuildUnderlay,
    // Like kRebuildUnderlay but with a FULL refresh. Used when an overlay is
    // dismissed: the area it covered must be cleared from the panel, and a
    // partial refresh leaves e-paper ghosting there.
    kRebuildUnderlayFull,
    // Recomposite overlays over the cached underlay snapshot (partial). Used for
    // in-overlay updates that don't change which overlays are visible.
    kReuseUnderlaySnapshot,
};

// Coalescing helpers, shared by the in-task command queue (display_service) and the
// keyed UI-refresh queue (ui_refresh_runtime) so both merge identically.
RefreshMode MergeRefreshMode(RefreshMode lhs, RefreshMode rhs);
RefreshRequest MergeRefreshRequest(const RefreshRequest& lhs, const RefreshRequest& rhs);
OverlayRefreshPolicy MergeOverlayRefreshPolicy(OverlayRefreshPolicy lhs,
                                               OverlayRefreshPolicy rhs);

esp_err_t Init();
bool IsInitialized();
int PortraitWidth();
int PortraitHeight();
ScreenId GetCurrentScreen();
esp_err_t SetStatusBarState(const epaper_ui::StatusBarState& state);
esp_err_t SetGlobalFooterState(const epaper_ui::GlobalFooterState& state);
esp_err_t SetSettingsPageState(const epaper_ui::SettingsPageState& state);
esp_err_t SetWifiPageState(const epaper_ui::WifiPageState& state);
esp_err_t SetTimePageState(const epaper_ui::TimePageState& state);
esp_err_t SetDashboardPageState(const epaper_ui::DashboardPageState& state);
esp_err_t SetLockScreenState(const epaper_ui::LockScreenState& state);
esp_err_t SetKeyboardState(const epaper_ui::KeyboardState& state);
esp_err_t SetShutdownModalState(const epaper_ui::ShutdownModalState& state);
esp_err_t SetStorageModalState(const epaper_ui::StorageModalState& state);
esp_err_t SetSelectModalState(const epaper_ui::SelectModalState& state);
esp_err_t SetToastState(const epaper_ui::ToastState& state);
// The optional `source` tag names what invoked the refresh; it is echoed in the
// "Display command requested" log to make refresh provenance traceable.
esp_err_t SetCurrentScreen(ScreenId screen,
                           RefreshMode refresh_mode = RefreshMode::kPartial,
                           const char* source = "?");
esp_err_t RequestOverlayRefresh(
    OverlayRefreshPolicy policy = OverlayRefreshPolicy::kRebuildUnderlay,
    const char* source = "?");
esp_err_t RequestRefreshCurrentScreen(
    const RefreshRequest& refresh_request, const char* source = "?");
esp_err_t RequestRefreshCurrentScreen(RefreshMode refresh_mode = RefreshMode::kPartial,
                                      const char* source = "?");
esp_err_t RefreshCurrentScreen(const RefreshRequest& refresh_request);
esp_err_t RefreshCurrentScreen(RefreshMode refresh_mode = RefreshMode::kPartial);
esp_err_t EnterDisplaySleep();
esp_err_t EnterLightSleep();
esp_err_t WakeDisplay();
esp_err_t RecoverAfterLightSleep();
bool IsRefreshInProgress();

}  // namespace display_service

#endif  // DISPLAY_SERVICE_H_
