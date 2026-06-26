#include "epaper_ui/menu_item.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

}  // namespace

UiRect MenuItemBounds(int origin_x, int origin_y, const MenuItemStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.height)};
}

void DrawMenuItem(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const MenuItemState& state,
                  const MenuItemStyle& style)
{
    const UiRect bounds = MenuItemBounds(origin_x, origin_y, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            bounds, kControlCornerRadius, background_color);

    const int border = ClampPositive(style.bottom_border_thickness);
    if (border > 0) {
        const int inset = std::min(kControlCornerRadius, bounds.width / 2);
        const int separator_width = std::max(0, bounds.width - (2 * inset));
        if (separator_width > 0) {
            const int thickness = std::min(border, bounds.height);
            const UiRect separator = {bounds.x + inset, bounds.bottom() - thickness,
                                      separator_width, thickness};
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             separator, style.border_color);
        }
    }

    if (state.shows_badge) {
        BadgeState badge = state.badge;
        if (state.selected) {
            badge.inverse = true;  // white pill so it reads on the inverted row
        }
        const UiRect badge_bounds = BadgeBounds(0, 0, badge, style.badge);
        DrawBadge(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                  bounds.right() - ClampPositive(style.horizontal_padding) - badge_bounds.width,
                  bounds.y + CenterOffset(bounds.height, badge_bounds.height), badge, style.badge);
    }

    if (!state.label_text.empty()) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           bounds.x + ClampPositive(style.horizontal_padding),
                           bounds.y + CenterOffset(bounds.height, LineHeight(style.role)),
                           state.label_text, style.role, text_color);
    }
}

}  // namespace epaper_ui
