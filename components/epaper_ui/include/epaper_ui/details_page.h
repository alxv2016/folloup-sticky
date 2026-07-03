#ifndef EPAPER_UI_DETAILS_PAGE_H_
#define EPAPER_UI_DETAILS_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/list_item_header.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/scroll_container.h"
#include "epaper_ui/status_bar.h"

namespace epaper_ui {

struct DetailsPageState {
    int navigation_focus_index = -1;
    std::string title_text = "Details";
    ListItemHeaderState recording_header = {};
    ScrollContainerState scroll_container = {};
    ButtonState back_button = {};

    bool operator==(const DetailsPageState& other) const = default;
};

bool HitTestDetailsScrollContainer(int portrait_width,
                                   int portrait_height,
                                   const DetailsPageState& state,
                                   int x,
                                   int y);
bool HitTestDetailsBackButton(int portrait_width,
                              int portrait_height,
                              const DetailsPageState& state,
                              int x,
                              int y);
void DrawDetailsPage(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     const DetailsPageState& state,
                     const StatusBarState& status_bar_state,
                     const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_DETAILS_PAGE_H_
