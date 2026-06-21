#ifndef DISPLAY_SERVICE_H_
#define DISPLAY_SERVICE_H_

#include "epaper_ui/lock_screen.h"
#include "epaper_ui/status_bar.h"
#include "esp_err.h"

namespace display_service {

enum class ScreenId {
    kHome,
    kLockScreen,
};

enum class DemoSelection {
    kTop,
};

enum class RefreshMode {
    kPartial,
    kFull,
};

esp_err_t Init();
bool IsInitialized();
ScreenId GetCurrentScreen();
esp_err_t SetStatusBarState(const epaper_ui::StatusBarState& state);
esp_err_t SetLockScreenState(const epaper_ui::LockScreenState& state);
esp_err_t SetCurrentScreen(ScreenId screen,
                           RefreshMode refresh_mode = RefreshMode::kPartial);
esp_err_t SelectDemoSelection(DemoSelection selection,
                              RefreshMode refresh_mode = RefreshMode::kPartial);
esp_err_t RequestRefreshCurrentScreen(RefreshMode refresh_mode = RefreshMode::kPartial);
esp_err_t RefreshCurrentScreen(RefreshMode refresh_mode = RefreshMode::kPartial);
esp_err_t EnterDisplaySleep();
esp_err_t EnterLightSleep();
esp_err_t WakeDisplay();
esp_err_t RecoverAfterLightSleep();
bool IsRefreshInProgress();

}  // namespace display_service

#endif  // DISPLAY_SERVICE_H_
