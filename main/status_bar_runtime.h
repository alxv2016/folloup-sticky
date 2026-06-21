#ifndef STATUS_BAR_RUNTIME_H_
#define STATUS_BAR_RUNTIME_H_

#include "display_service.h"
#include "epaper_ui/status_bar.h"
#include "esp_err.h"

namespace status_bar_runtime {

void SetSleepIndicatorVisible(bool visible);
void SetShutdownIndicatorVisible(bool visible);
epaper_ui::StatusBarState BuildState();
esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRefreshNow(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);

}  // namespace status_bar_runtime

#endif  // STATUS_BAR_RUNTIME_H_
