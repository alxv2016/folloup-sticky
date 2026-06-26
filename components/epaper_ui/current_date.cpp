#include "epaper_ui/current_date.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {

UiRect CurrentDateBounds(int origin_x,
                         int origin_y,
                         const CurrentDateState& state,
                         const CurrentDateStyle& style)
{
    const bool has_weekday = !state.weekday_text.empty();
    const bool has_date = !state.date_text.empty();
    const bool has_separator = has_weekday && has_date;

    const int weekday_width = has_weekday ? MeasureText(style.weekday_role, state.weekday_text) : 0;
    const int date_width = has_date ? MeasureText(style.date_role, state.date_text) : 0;
    const int separator_width = has_separator ? ClampPositive(style.separator_width) : 0;
    const int segment_count =
        static_cast<int>(has_weekday) + static_cast<int>(has_separator) + static_cast<int>(has_date);
    const int gap_count = std::max(0, segment_count - 1);

    const int width =
        weekday_width + separator_width + date_width + (gap_count * ClampPositive(style.segment_gap));
    const int text_height = std::max(has_weekday ? LineHeight(style.weekday_role) : 0,
                                     has_date ? LineHeight(style.date_role) : 0);
    const int height = std::max(text_height, ClampPositive(style.separator_height));
    return {origin_x, origin_y, width, height};
}

void DrawCurrentDate(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const CurrentDateState& state,
                     const CurrentDateStyle& style)
{
    const bool has_weekday = !state.weekday_text.empty();
    const bool has_date = !state.date_text.empty();
    if (!has_weekday && !has_date) {
        return;
    }

    const UiRect bounds = CurrentDateBounds(origin_x, origin_y, state, style);
    const int gap = ClampPositive(style.segment_gap);
    int cursor_x = bounds.x;

    if (has_weekday) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           cursor_x,
                           bounds.y + CenterOffset(bounds.height, LineHeight(style.weekday_role)),
                           state.weekday_text, style.weekday_role, style.text_color);
        cursor_x += MeasureText(style.weekday_role, state.weekday_text);
    }

    if (has_weekday && has_date) {
        cursor_x += gap;
        const int separator_height = ClampPositive(style.separator_height);
        const UiRect separator = {cursor_x, bounds.y + CenterOffset(bounds.height, separator_height),
                                  ClampPositive(style.separator_width), separator_height};
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         separator, style.separator_color);
        cursor_x += ClampPositive(style.separator_width) + gap;
    }

    if (has_date) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           cursor_x,
                           bounds.y + CenterOffset(bounds.height, LineHeight(style.date_role)),
                           state.date_text, style.date_role, style.text_color);
    }
}

}  // namespace epaper_ui
