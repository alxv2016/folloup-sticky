#include "epaper_ui/dashboard_page.h"

#include <algorithm>
#include <array>

#include "epaper_ui/menu_container.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kMargin = design::spacing::k16;
constexpr int kContentTopGap = design::spacing::k32;
constexpr int kWelcomeMiddleGap = design::spacing::k48;
constexpr int kMiddleMenuGap = design::spacing::k8;
constexpr auto kProgressLabelRole = design::TypographyRole::kLabelSmallBlack;

constexpr std::array<const char*, kDashboardMenuItemCount> kMenuLabels = {
    "Follow up", "Summarize", "Vibe check", "Notes", "Todos",
};

int PageWidth(int portrait_width)
{
    return std::max(0, portrait_width - (2 * kMargin));
}

WelcomeMessageStyle WelcomeStyle(int width)
{
    WelcomeMessageStyle style = {};
    style.title_max_width = width;
    return style;
}

CompletionBannerStyle BannerStyle(int width)
{
    CompletionBannerStyle style = {};
    style.width = width;
    return style;
}

ProgressBarStyle ProgressStyle(int width)
{
    ProgressBarStyle style = {};
    style.label_role = kProgressLabelRole;
    style.width = width;
    return style;
}

MenuContainerState MenuState(const DashboardPageState& state)
{
    return {state.menu.selected_index, kDashboardMenuItemCount};
}

MenuContainerStyle MenuStyle(int width)
{
    MenuContainerStyle style = {};
    style.direction = MenuContainerDirection::kVertical;
    style.sizing = MenuContainerSizing::kFixedItemExtent;
    style.width = width;
    style.item_height = design::menu_item::kHeight;
    style.item_gap = 0;
    return style;
}

struct Layout {
    UiRect welcome = {};
    bool shows_banner = false;
    UiRect middle = {};
    int menu_y = 0;
};

Layout BuildLayout(int portrait_width, int portrait_height, const DashboardPageState& state)
{
    (void)portrait_height;
    const int page_width = PageWidth(portrait_width);
    const int content_top = StatusBarHeight() + kContentTopGap;

    Layout layout = {};
    layout.welcome =
        WelcomeMessageBounds(kMargin, content_top, state.welcome_message, WelcomeStyle(page_width));
    const int middle_y = layout.welcome.bottom() + kWelcomeMiddleGap;
    layout.shows_banner = state.shows_completion_banner;
    if (layout.shows_banner) {
        layout.middle = CompletionBannerBounds(kMargin, middle_y, state.completion_banner,
                                               BannerStyle(page_width));
    } else {
        layout.middle =
            ProgressBarBounds(kMargin, middle_y, state.current_progress, ProgressStyle(page_width));
    }
    layout.menu_y = layout.middle.bottom() + kMiddleMenuGap;
    return layout;
}

bool MenuItemShowsBadge(const DashboardPageMenuState& menu, int index)
{
    switch (index) {
        case 0:
            return menu.shows_follow_up_badge;
        case 3:
            return menu.shows_notes_badge;
        case 4:
            return menu.shows_todos_badge;
        default:
            return false;
    }
}

BadgeState MenuItemBadge(const DashboardPageMenuState& menu, int index)
{
    switch (index) {
        case 0:
            return {menu.follow_up_badge_text, false};
        case 3:
            return {menu.notes_badge_text, false};
        case 4:
            return {menu.todos_badge_text, false};
        default:
            return {};
    }
}

}  // namespace

const char* DashboardMenuItemLabel(int index)
{
    if (index < 0 || index >= kDashboardMenuItemCount) {
        return "";
    }
    return kMenuLabels[static_cast<size_t>(index)];
}

UiRect DashboardMenuItemBounds(int portrait_width,
                               int portrait_height,
                               const DashboardPageState& state,
                               int index)
{
    if (index < 0 || index >= kDashboardMenuItemCount) {
        return {};
    }
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    return MenuContainerItemBounds(kMargin, layout.menu_y, MenuState(state),
                                   MenuStyle(PageWidth(portrait_width)), index);
}

bool HitTestDashboardMenuItem(int portrait_width,
                              int portrait_height,
                              const DashboardPageState& state,
                              int x,
                              int y,
                              int* index)
{
    if (index != nullptr) {
        *index = -1;
    }
    for (int candidate = 0; candidate < kDashboardMenuItemCount; ++candidate) {
        const UiRect bounds =
            DashboardMenuItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (index != nullptr) {
                *index = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawDashboardPage(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       const DashboardPageState& state,
                       const StatusBarState& status_bar_state,
                       const GlobalFooterState& footer_state)
{
    if (framebuffer == nullptr) {
        return;
    }

    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     {0, 0, portrait_width, portrait_height}, design::color::kWhite);
    DrawStatusBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  status_bar_state);

    const int page_width = PageWidth(portrait_width);
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);

    DrawWelcomeMessage(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       layout.welcome.x, layout.welcome.y, state.welcome_message,
                       WelcomeStyle(page_width));

    if (layout.shows_banner) {
        DrawCompletionBanner(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             layout.middle.x, layout.middle.y, state.completion_banner,
                             BannerStyle(page_width));
    } else {
        DrawProgressBar(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                        layout.middle.x, layout.middle.y, state.current_progress,
                        ProgressStyle(page_width));
    }

    for (int index = 0; index < kDashboardMenuItemCount; ++index) {
        const UiRect bounds = MenuContainerItemBounds(kMargin, layout.menu_y, MenuState(state),
                                                      MenuStyle(page_width), index);
        MenuItemState item = {};
        item.label_text = DashboardMenuItemLabel(index);
        item.selected = index == state.menu.selected_index;
        item.shows_badge = MenuItemShowsBadge(state.menu, index);
        item.badge = MenuItemBadge(state.menu, index);

        MenuItemStyle item_style = {};
        item_style.width = bounds.width;
        item_style.height = bounds.height;
        if (index == kDashboardMenuItemCount - 1) {
            item_style.bottom_border_thickness = 0;  // no separator under the last row
        }
        DrawMenuItem(framebuffer, raw_width, raw_height, portrait_width, portrait_height, bounds.x,
                     bounds.y, item, item_style);
    }

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
