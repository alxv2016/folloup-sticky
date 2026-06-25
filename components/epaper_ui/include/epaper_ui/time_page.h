#ifndef EPAPER_UI_TIME_PAGE_H_
#define EPAPER_UI_TIME_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/button.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/select_input.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/text_input.h"
#include "epaper_ui/time_input.h"

namespace epaper_ui {

enum class TimePageItemId : uint8_t {
    kNone = 0,
    kTimezone,
    kHour,
    kMinute,
    kMeridiem,
    kMonth,
    kDay,
    kYear,
    kSave,
};

struct TimePageState {
    int navigation_focus_index = -1;
    std::string title_text = "Time setting";
    std::string manual_heading_text = "Set manual time";
    SelectInputState timezone = {};
    TimeInputState hour = {};
    TimeInputState minute = {};
    ButtonState meridiem = {};
    TimeInputState month = {};
    TimeInputState day = {};
    TimeInputState year = {};
    ButtonState save = {};

    bool operator==(const TimePageState& other) const = default;
};

UiRect TimePageItemBounds(int portrait_width,
                          int portrait_height,
                          const TimePageState& state,
                          TimePageItemId item);
UiRect TimePageItemVisualBounds(int portrait_width,
                                int portrait_height,
                                const TimePageState& state,
                                TimePageItemId item);
bool HitTestTimePageItem(int portrait_width,
                         int portrait_height,
                         const TimePageState& state,
                         int x,
                         int y,
                         TimePageItemId* item);
void DrawTimePage(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const TimePageState& state,
                  const StatusBarState& status_bar_state,
                  const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TIME_PAGE_H_
