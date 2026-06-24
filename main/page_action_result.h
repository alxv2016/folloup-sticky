#ifndef PAGE_ACTION_RESULT_H_
#define PAGE_ACTION_RESULT_H_

#include "epaper_ui/overlay_geometry.h"
#include "footer_runtime.h"

namespace page_actions {

struct FocusUpdateOutcome {
    bool handled = false;
    bool apply_page_state = false;
    bool sync_footer_projection = false;
};

struct FocusMoveOutcome {
    bool handled = false;
    bool play_navigation_cue = false;
    bool apply_page_state = false;
    bool sync_footer_projection = false;
};

}  // namespace page_actions

#endif  // PAGE_ACTION_RESULT_H_
