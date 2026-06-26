#include "epaper_ui/badge.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kBadgeCornerRadius = 4;

}  // namespace

UiRect BadgeBounds(int origin_x, int origin_y, const BadgeState& state, const BadgeStyle& style)
{
    const int horizontal_padding = ClampPositive(style.horizontal_padding);
    const int text_width = state.label_text.empty() ? 0 : MeasureText(style.role, state.label_text);
    const int requested_width =
        style.width > 0 ? style.width : text_width + (2 * horizontal_padding);
    return {origin_x, origin_y, std::max(ClampPositive(style.min_width), requested_width),
            ClampPositive(style.height)};
}

void DrawBadge(uint8_t* framebuffer,
               int raw_width,
               int raw_height,
               int portrait_width,
               int portrait_height,
               int origin_x,
               int origin_y,
               const BadgeState& state,
               const BadgeStyle& style)
{
    if (style.height <= 0) {
        return;
    }

    const UiRect bounds = BadgeBounds(origin_x, origin_y, state, style);
    const uint8_t background_color =
        state.inverse ? style.inverse_background_color : style.background_color;
    const uint8_t text_color = state.inverse ? style.inverse_text_color : style.text_color;

    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            bounds, kBadgeCornerRadius, background_color);
    if (state.inverse) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, bounds, kBadgeCornerRadius,
                                  style.border_thickness, style.border_color);
    }

    if (state.label_text.empty()) {
        return;
    }

    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       bounds.x + CenterOffset(bounds.width, MeasureText(style.role,
                                                                         state.label_text)),
                       bounds.y + CenterOffset(bounds.height, LineHeight(style.role)),
                       state.label_text, style.role, text_color);
}

}  // namespace epaper_ui
