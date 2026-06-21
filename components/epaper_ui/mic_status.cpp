#include "epaper_ui/mic_status.h"

#include <algorithm>

#include "epaper_ui/button_icon.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

const EmbeddedImageAsset* ResolveIcon(const EmbeddedImageAsset* idle_icon,
                                      const EmbeddedImageAsset* focused_icon,
                                      bool focused)
{
    return focused ? focused_icon : idle_icon;
}

}  // namespace

UiRect MicStatusBounds(int origin_x,
                       int origin_y,
                       const MicStatusState& state,
                       const MicStatusStyle& style)
{
    const int size = ClampPositive(style.size);
    const int label_width =
        (state.focused && !state.label_text.empty()) ? MeasureText(style.label_role, state.label_text)
                                                     : 0;
    const int gap = label_width > 0 ? ClampPositive(style.label_gap) : 0;
    const int trailing_padding = label_width > 0 ? ClampPositive(style.trailing_padding) : 0;
    return {origin_x, origin_y, size + gap + label_width + trailing_padding, size};
}

void DrawMicStatus(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   int origin_x,
                   int origin_y,
                   const MicStatusState& state,
                   const MicStatusStyle& style,
                   const EmbeddedImageAsset* idle_icon,
                   const EmbeddedImageAsset* focused_icon)
{
    const EmbeddedImageAsset* icon = ResolveIcon(idle_icon, focused_icon, state.focused);
    if (icon == nullptr || icon->data == nullptr || style.size <= 0) {
        return;
    }

    const UiRect bounds = MicStatusBounds(origin_x, origin_y, state, style);
    const uint8_t background_color =
        state.focused ? style.focused_background_color : style.background_color;
    const uint8_t icon_color = state.focused ? style.focused_icon_color : style.icon_color;

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            bounds,
                            4,
                            background_color);
    DrawRoundedPortraitBorder(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              bounds,
                              4,
                              style.border_thickness,
                              style.border_color);

    ButtonIconStyle icon_style = {};
    icon_style.size = style.size;
    icon_style.icon_size = style.icon_size;
    icon_style.background_color = background_color;
    icon_style.selected_background_color = background_color;
    icon_style.border_thickness = 0;
    icon_style.icon_color = icon_color;
    icon_style.selected_icon_color = icon_color;
    icon_style.outline_icon_when_unselected = style.outline_idle_icon && !state.focused;

    DrawButtonIcon(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   origin_x,
                   origin_y,
                   {.asset = icon, .selected = true},
                   icon_style);

    if (!state.focused || state.label_text.empty()) {
        return;
    }

    const int text_x = origin_x + ClampPositive(style.size) + ClampPositive(style.label_gap);
    const int text_y =
        origin_y + CenterOffset(bounds.height, LineHeight(style.label_role));
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       text_x,
                       text_y,
                       state.label_text,
                       style.label_role,
                       style.focused_text_color);
}

}  // namespace epaper_ui
