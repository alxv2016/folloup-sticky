#include "epaper_ui/checkbox.h"

#include <algorithm>

#include "asset_types.h"
#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

template <typename DrawFn>
void ForEachOutlineOffset(int stroke_thickness, DrawFn&& fn)
{
    const int thickness = ClampPositive(stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            fn(dx, dy);
        }
    }
}

const EmbeddedImageAsset* ResolveAsset(bool checked)
{
    return project_assets::GetIcon(checked ? EmbeddedIconId::kCheckboxChecked
                                           : EmbeddedIconId::kCheckboxUnchecked);
}

}  // namespace

UiRect CheckboxBounds(int origin_x, int origin_y, const CheckboxStyle& style)
{
    const int size = ClampPositive(style.size);
    return {origin_x, origin_y, size, size};
}

void DrawCheckbox(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const CheckboxState& state,
                  const CheckboxStyle& style)
{
    if (style.size <= 0) {
        return;
    }
    const EmbeddedImageAsset* asset = ResolveAsset(state.checked);
    if (asset == nullptr || asset->data == nullptr) {
        return;
    }

    const UiRect bounds = CheckboxBounds(origin_x, origin_y, style);
    const bool show_background =
        state.selected ? style.selected_background_visible : style.background_visible;
    if (show_background) {
        const uint8_t background_color =
            state.selected ? style.selected_background_color : style.background_color;
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                bounds, ClampPositive(style.background_corner_radius),
                                background_color);
    }

    const uint8_t icon_color = state.selected ? style.selected_icon_color : style.icon_color;
    const int draw_width = std::min(bounds.width, static_cast<int>(asset->width));
    const int draw_height = std::min(bounds.height, static_cast<int>(asset->height));
    const int x = bounds.x + CenterOffset(bounds.width, draw_width);
    const int y = bounds.y + CenterOffset(bounds.height, draw_height);
    if (state.selected && style.selected_content_outlined) {
        ForEachOutlineOffset(style.selected_content_stroke_thickness, [&](int dx, int dy) {
            DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, x + dx, y + dy, asset,
                                  style.selected_content_outline_color);
        });
    }
    DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width, portrait_height, x, y,
                          asset, icon_color);
}

}  // namespace epaper_ui
