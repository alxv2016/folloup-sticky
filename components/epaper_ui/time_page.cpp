#include "epaper_ui/time_page.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH1;
constexpr auto kManualHeadingRole = design::TypographyRole::kHeadingH2;
constexpr int kSideInset = design::spacing::k16;
constexpr int kTopGap = design::spacing::k24;
constexpr int kHeadingBottomPadding = design::spacing::k24;
constexpr int kSectionGap = design::spacing::k16;
constexpr int kSectionBlockGap = design::spacing::k24;
constexpr int kColumnGap = design::spacing::k12;
constexpr int kManualHeadingGap = design::spacing::k16;

struct Layout {
    UiRect timezone = {};
    int manual_heading_y = 0;
    UiRect hour = {};
    UiRect minute = {};
    UiRect meridiem = {};
    UiRect month = {};
    UiRect day = {};
    UiRect year = {};
    UiRect save = {};
};

TextInputStyle FieldStyle(int width)
{
    TextInputStyle style = {};
    style.width = width;
    return style;
}

ButtonStyle MeridiemStyle(int width)
{
    ButtonStyle style = {};
    style.width = width;
    style.center_label = true;
    return style;
}

ButtonStyle SaveStyle(int width)
{
    ButtonStyle style = {};
    style.variant = ButtonVariant::kPrimary;
    style.width = width;
    style.center_label = true;
    return style;
}

int Bottom3(const UiRect& a, const UiRect& b, const UiRect& c)
{
    return std::max(std::max(a.bottom(), b.bottom()), c.bottom());
}

Layout BuildLayout(int portrait_width, int portrait_height, const TimePageState& state)
{
    (void)portrait_height;
    const int page_x = kSideInset;
    const int page_width = std::max(0, portrait_width - (2 * kSideInset));
    const int title_y = StatusBarHeight() + kTopGap;
    const int title_bottom = title_y + LineHeight(kTitleRole);

    int y = title_bottom + kHeadingBottomPadding;

    Layout layout = {};
    layout.timezone = SelectInputBounds(page_x, y, state.timezone, FieldStyle(page_width));
    y = layout.timezone.bottom() + kSectionBlockGap;

    layout.manual_heading_y = y;
    y = layout.manual_heading_y + LineHeight(kManualHeadingRole) + kManualHeadingGap;

    const int column = std::max(0, (page_width - (2 * kColumnGap)) / 3);
    const int column2_x = page_x + column + kColumnGap;
    const int column3_x = page_x + (2 * (column + kColumnGap));

    layout.hour = TimeInputBounds(page_x, y, state.hour, FieldStyle(column));
    layout.minute = TimeInputBounds(column2_x, y, state.minute, FieldStyle(column));
    layout.meridiem = ButtonBounds(column3_x, y, state.meridiem, MeridiemStyle(column));
    y = Bottom3(layout.hour, layout.minute, layout.meridiem) + kSectionGap;

    layout.month = TimeInputBounds(page_x, y, state.month, FieldStyle(column));
    layout.day = TimeInputBounds(column2_x, y, state.day, FieldStyle(column));
    layout.year = TimeInputBounds(column3_x, y, state.year, FieldStyle(column));
    y = Bottom3(layout.month, layout.day, layout.year) + kSectionBlockGap;

    layout.save = ButtonBounds(page_x, y, state.save, SaveStyle(page_width));
    return layout;
}

}  // namespace

UiRect TimePageItemBounds(int portrait_width,
                          int portrait_height,
                          const TimePageState& state,
                          TimePageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case TimePageItemId::kTimezone:
            return layout.timezone;
        case TimePageItemId::kHour:
            return layout.hour;
        case TimePageItemId::kMinute:
            return layout.minute;
        case TimePageItemId::kMeridiem:
            return layout.meridiem;
        case TimePageItemId::kMonth:
            return layout.month;
        case TimePageItemId::kDay:
            return layout.day;
        case TimePageItemId::kYear:
            return layout.year;
        case TimePageItemId::kSave:
            return layout.save;
        case TimePageItemId::kNone:
        default:
            return {};
    }
}

UiRect TimePageItemVisualBounds(int portrait_width,
                                int portrait_height,
                                const TimePageState& state,
                                TimePageItemId item)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    switch (item) {
        case TimePageItemId::kTimezone:
            return SelectInputVisualBounds(layout.timezone.x, layout.timezone.y, state.timezone,
                                           FieldStyle(layout.timezone.width));
        case TimePageItemId::kHour:
            return TimeInputVisualBounds(layout.hour.x, layout.hour.y, state.hour,
                                         FieldStyle(layout.hour.width));
        case TimePageItemId::kMinute:
            return TimeInputVisualBounds(layout.minute.x, layout.minute.y, state.minute,
                                         FieldStyle(layout.minute.width));
        case TimePageItemId::kMonth:
            return TimeInputVisualBounds(layout.month.x, layout.month.y, state.month,
                                         FieldStyle(layout.month.width));
        case TimePageItemId::kDay:
            return TimeInputVisualBounds(layout.day.x, layout.day.y, state.day,
                                         FieldStyle(layout.day.width));
        case TimePageItemId::kYear:
            return TimeInputVisualBounds(layout.year.x, layout.year.y, state.year,
                                         FieldStyle(layout.year.width));
        case TimePageItemId::kMeridiem:
            return layout.meridiem;
        case TimePageItemId::kSave:
            return layout.save;
        case TimePageItemId::kNone:
        default:
            return {};
    }
}

bool HitTestTimePageItem(int portrait_width,
                         int portrait_height,
                         const TimePageState& state,
                         int x,
                         int y,
                         TimePageItemId* item)
{
    if (item != nullptr) {
        *item = TimePageItemId::kNone;
    }

    constexpr TimePageItemId kItems[] = {
        TimePageItemId::kTimezone, TimePageItemId::kHour,     TimePageItemId::kMinute,
        TimePageItemId::kMeridiem, TimePageItemId::kMonth,    TimePageItemId::kDay,
        TimePageItemId::kYear,     TimePageItemId::kSave,
    };
    for (TimePageItemId candidate : kItems) {
        const UiRect bounds = TimePageItemBounds(portrait_width, portrait_height, state, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (item != nullptr) {
                *item = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawTimePage(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const TimePageState& state,
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

    const Layout layout = BuildLayout(portrait_width, portrait_height, state);
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       kSideInset, StatusBarHeight() + kTopGap, state.title_text, kTitleRole,
                       design::color::kBlack);

    DrawSelectInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                    layout.timezone.x, layout.timezone.y, state.timezone,
                    FieldStyle(layout.timezone.width));

    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       kSideInset, layout.manual_heading_y, state.manual_heading_text,
                       kManualHeadingRole, design::color::kBlack);

    DrawTimeInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.hour.x, layout.hour.y, state.hour, FieldStyle(layout.hour.width));
    DrawTimeInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.minute.x, layout.minute.y, state.minute, FieldStyle(layout.minute.width));
    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
               layout.meridiem.x, layout.meridiem.y, state.meridiem,
               MeridiemStyle(layout.meridiem.width));

    DrawTimeInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.month.x, layout.month.y, state.month, FieldStyle(layout.month.width));
    DrawTimeInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.day.x, layout.day.y, state.day, FieldStyle(layout.day.width));
    DrawTimeInput(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  layout.year.x, layout.year.y, state.year, FieldStyle(layout.year.width));

    DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
               layout.save.x, layout.save.y, state.save, SaveStyle(layout.save.width));

    DrawGlobalFooter(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     footer_state);
}

}  // namespace epaper_ui
