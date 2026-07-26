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
// Overload carrying a full RefreshRequest, so callers can ask for RefreshScope::kRegion.
// A region refresh routes through RefreshChangedRegion, which compares the framebuffer
// against the glass and does nothing when they match -- unlike the screen-scope partial,
// which re-inits the panel and drives it regardless.
esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request);
esp_err_t UpdateDisplayStateAndRefreshNow(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);

}  // namespace status_bar_runtime

#endif  // STATUS_BAR_RUNTIME_H_
