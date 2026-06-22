#include "epaper_ui/menu_toggle.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {

UiRect MenuToggleBounds(int origin_x, int origin_y, const MenuToggleStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.height)};
}

void DrawMenuToggle(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const MenuToggleState& state,
                    const MenuToggleStyle& style)
{
    const UiRect bounds = MenuToggleBounds(origin_x, origin_y, style);
    if (bounds.IsEmpty()) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     bounds,
                     style.background_color);

    const int border_height = ClampPositive(style.bottom_border_thickness);
    if (border_height > 0) {
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         {bounds.x,
                          bounds.bottom() - std::min(border_height, bounds.height),
                          bounds.width,
                          std::min(border_height, bounds.height)},
                         style.border_color);
    }

    ToggleStyle toggle_style = style.toggle;
    const UiRect toggle_bounds = ToggleBounds(0, 0, toggle_style);
    const int toggle_x =
        bounds.right() - ClampPositive(style.horizontal_padding) - toggle_bounds.width;
    const int toggle_y = bounds.y + CenterOffset(bounds.height, toggle_bounds.height);
    DrawToggle(framebuffer,
               raw_width,
               raw_height,
               portrait_width,
               portrait_height,
               toggle_x,
               toggle_y,
               state.toggle_state,
               toggle_style);

    if (state.label_text.empty()) {
        return;
    }

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       bounds.x + ClampPositive(style.horizontal_padding),
                       bounds.y + CenterOffset(bounds.height, LineHeight(style.role)),
                       state.label_text,
                       style.role,
                       style.text_color);
}

}  // namespace epaper_ui
