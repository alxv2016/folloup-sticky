#ifndef FOOTER_RUNTIME_H_
#define FOOTER_RUNTIME_H_

#include <mutex>

#include "display_service.h"
#include "epaper_ui/global_footer.h"
#include "esp_err.h"

namespace footer_runtime {

void SetBaseState(const epaper_ui::GlobalFooterState& state);
epaper_ui::GlobalFooterState BuildState();
esp_err_t UpdateDisplayState();
esp_err_t UpdateDisplayStateAndRequestRefresh(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);
esp_err_t UpdateDisplayStateAndRefreshNow(
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial);

}  // namespace footer_runtime

#endif  // FOOTER_RUNTIME_H_
