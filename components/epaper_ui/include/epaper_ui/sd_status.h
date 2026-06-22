#ifndef EPAPER_UI_SD_STATUS_H_
#define EPAPER_UI_SD_STATUS_H_

#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/progress_bar.h"

namespace epaper_ui {

struct SdStatusState {
    bool has_sd_card = false;
    std::string free_space_text = {};
    int used_percent = 0;
};

struct SdStatusStyle {
    int max_width = design::sd_status::kMaxWidth;
    int icon_size = design::sd_status::kIconSize;
    int gap = design::sd_status::kGap;
    ProgressBarStyle progress = {};
};

UiRect SdStatusBounds(int origin_x, int origin_y, const SdStatusState& state, const SdStatusStyle& style);
void DrawSdStatus(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const SdStatusState& state,
                  const SdStatusStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SD_STATUS_H_
