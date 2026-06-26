#ifndef EPAPER_UI_DASHBOARD_PAGE_H_
#define EPAPER_UI_DASHBOARD_PAGE_H_

#include <cstdint>
#include <string>

#include "epaper_ui/completion_banner.h"
#include "epaper_ui/global_footer.h"
#include "epaper_ui/menu_item.h"
#include "epaper_ui/progress_bar.h"
#include "epaper_ui/status_bar.h"
#include "epaper_ui/welcome_message.h"

namespace epaper_ui {

// The dashboard's main menu has five fixed items; Follow up / Notes / Todos can show a badge.
inline constexpr int kDashboardMenuItemCount = 5;

struct DashboardPageMenuState {
    int selected_index = -1;
    bool shows_follow_up_badge = false;
    bool shows_notes_badge = false;
    bool shows_todos_badge = false;
    std::string follow_up_badge_text = "New";
    std::string notes_badge_text = "New";
    std::string todos_badge_text = "New";

    bool operator==(const DashboardPageMenuState& other) const = default;
};

struct DashboardPageState {
    int navigation_focus_index = -1;
    WelcomeMessageState welcome_message = {};
    bool shows_completion_banner = false;
    CompletionBannerState completion_banner = {};
    ProgressBarState current_progress = {};
    DashboardPageMenuState menu = {};

    bool operator==(const DashboardPageState& other) const = default;
};

// Label + badge-support for each fixed menu slot (0..kDashboardMenuItemCount-1).
const char* DashboardMenuItemLabel(int index);

UiRect DashboardMenuItemBounds(int portrait_width,
                               int portrait_height,
                               const DashboardPageState& state,
                               int index);
bool HitTestDashboardMenuItem(int portrait_width,
                              int portrait_height,
                              const DashboardPageState& state,
                              int x,
                              int y,
                              int* index);
void DrawDashboardPage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const DashboardPageState& state,
                       const StatusBarState& status_bar_state,
                       const GlobalFooterState& footer_state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_DASHBOARD_PAGE_H_
