#include "epaper_ui/progress_bar.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

UiRect InsetRect(const UiRect& rect, int inset)
{
    const int safe_inset = std::max(0, inset);
    return {rect.x + safe_inset,
            rect.y + safe_inset,
            std::max(0, rect.width - (2 * safe_inset)),
            std::max(0, rect.height - (2 * safe_inset))};
}

}  // namespace

UiRect ProgressBarBounds(int origin_x,
                         int origin_y,
                         const ProgressBarState&,
                         const ProgressBarStyle& style)
{
    const int width = ClampPositive(style.width);
    const int bar_height = ClampPositive(style.bar_height);
    const int meta_gap = ClampPositive(style.meta_gap);
    const int text_height =
        std::max(LineHeight(style.label_role), LineHeight(style.status_role));
    return {origin_x, origin_y, width, bar_height + meta_gap + text_height};
}

void DrawProgressBar(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const ProgressBarState& state,
                     const ProgressBarStyle& style)
{
    const UiRect bounds = ProgressBarBounds(origin_x, origin_y, state, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const int bar_height = ClampPositive(style.bar_height);
    const int meta_gap = ClampPositive(style.meta_gap);
    const int border_thickness = ClampPositive(style.border_thickness);
    const int clamped_progress = std::clamp(state.progress_percent, 0, 100);
    const UiRect track = {origin_x, origin_y, bounds.width, bar_height};

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            track,
                            ClampPositive(std::min(track.width, track.height) / 2),
                            style.border_color);
    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            InsetRect(track, border_thickness),
                            ClampPositive(std::max(0, std::min(track.width, track.height) / 2 - border_thickness)),
                            style.track_color);

    const UiRect track_inner = InsetRect(track, border_thickness);
    const int fill_width =
        track_inner.width > 0 ? (track_inner.width * clamped_progress + 50) / 100 : 0;
    const UiRect fill = {
        track_inner.x,
        track_inner.y,
        std::clamp(fill_width, 0, track_inner.width),
        track_inner.height,
    };
    if (!fill.IsEmpty()) {
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                fill,
                                ClampPositive(std::min(fill.width, fill.height) / 2),
                                style.fill_color);
    }

    const int meta_y = origin_y + bar_height + meta_gap;
    if (!state.label_text.empty()) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           origin_x,
                           meta_y,
                           state.label_text,
                           style.label_role,
                           style.text_color);
    }

    if (!state.status_text.empty()) {
        const int status_width = MeasureText(style.status_role, state.status_text);
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           origin_x + std::max(0, bounds.width - status_width),
                           meta_y,
                           state.status_text,
                           style.status_role,
                           style.text_color);
    }
}

}  // namespace epaper_ui
