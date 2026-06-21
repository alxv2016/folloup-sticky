#include "epaper_ui/button_icon.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

template <typename DrawFn>
void DrawOutlinedAsset(int origin_x, int origin_y, int stroke_thickness, DrawFn&& draw_fn)
{
    const int thickness = ClampPositive(stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            draw_fn(origin_x + dx, origin_y + dy, design::color::kWhite);
        }
    }
    draw_fn(origin_x, origin_y, design::color::kBlack);
}

}  // namespace

UiRect ButtonIconBounds(int origin_x, int origin_y, const ButtonIconStyle& style)
{
    const int size = ClampPositive(style.size);
    return {origin_x, origin_y, size, size};
}

void DrawButtonIcon(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const ButtonIconState& state,
                    const ButtonIconStyle& style)
{
    if (state.asset == nullptr || state.asset->data == nullptr || style.size <= 0) {
        return;
    }

    const UiRect bounds = ButtonIconBounds(origin_x, origin_y, style);
    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t icon_color = state.selected ? style.selected_icon_color : style.icon_color;

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            bounds,
                            kControlCornerRadius,
                            background_color);
    DrawRoundedPortraitBorder(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              bounds,
                              kControlCornerRadius,
                              style.border_thickness,
                              style.border_color);

    const int slot_size = ClampPositive(style.icon_size);
    const int draw_width = std::min(slot_size, static_cast<int>(state.asset->width));
    const int draw_height = std::min(slot_size, static_cast<int>(state.asset->height));
    const int slot_x = bounds.x + CenterOffset(bounds.width, slot_size);
    const int slot_y = bounds.y + CenterOffset(bounds.height, slot_size);
    const int icon_x = slot_x + CenterOffset(slot_size, draw_width);
    const int icon_y = slot_y + CenterOffset(slot_size, draw_height);

    if (state.selected || !style.outline_icon_when_unselected) {
        DrawPortraitMonoAsset(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              icon_x,
                              icon_y,
                              state.asset,
                              icon_color);
        return;
    }

    DrawOutlinedAsset(icon_x,
                      icon_y,
                      style.stroke_thickness,
                      [&](int draw_x, int draw_y, uint8_t tone) {
                          DrawPortraitMonoAsset(framebuffer,
                                                raw_width,
                                                raw_height,
                                                portrait_width,
                                                portrait_height,
                                                draw_x,
                                                draw_y,
                                                state.asset,
                                                tone);
                      });
}

}  // namespace epaper_ui
