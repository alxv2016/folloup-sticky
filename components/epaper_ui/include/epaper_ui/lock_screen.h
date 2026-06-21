#ifndef EPAPER_UI_LOCK_SCREEN_H_
#define EPAPER_UI_LOCK_SCREEN_H_

#include <string>

#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct LockScreenState {
    std::string hour_text = "--";
    std::string minute_text = "--";
    std::string weekday_text = {};
    std::string date_text = {};
};

void DrawLockScreen(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    const LockScreenState& state,
                    const StatusBarState& status_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_LOCK_SCREEN_H_
