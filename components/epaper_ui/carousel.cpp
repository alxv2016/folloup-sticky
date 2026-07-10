#include "epaper_ui/carousel.h"

#include <algorithm>

#include "asset_types.h"
#include "epaper_ui/button_icon.h"
#include "epaper_ui/status_bar.h"
#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

struct Layout {
    UiRect region = {};
    UiRect content = {};
    UiRect control_row = {};
    UiRect close = {};
    UiRect prev = {};
    UiRect next = {};
    int dots_x = 0;
    int dots_y = 0;
};

ButtonState CloseButtonState(const CarouselState& state, const CarouselStyle& style)
{
    return {.label_text = style.close_label, .selected = state.close_selected};
}

Layout BuildLayout(int portrait_width, int portrait_height, const CarouselState& state,
                   const CarouselStyle& style)
{
    Layout layout = {};
    const int status_height = StatusBarHeight();
    layout.region = {0, status_height, portrait_width,
                     std::max(0, portrait_height - status_height)};

    const int pad = ClampPositive(style.padding);
    const UiRect inner = {layout.region.x + pad, layout.region.y + pad,
                          std::max(0, layout.region.width - (2 * pad)),
                          std::max(0, layout.region.height - (2 * pad))};

    const int icon_size = ClampPositive(style.icon_button_size);
    const UiRect close_probe =
        ButtonBounds(0, 0, CloseButtonState(state, style), style.close_button);
    const int close_w = close_probe.width;
    const int close_h = close_probe.height;
    const int row_height = std::max(icon_size, close_h);

    layout.control_row = {inner.x, inner.bottom() - row_height, inner.width, row_height};

    // Right controls, left -> right: [Close] <prev> <next>, right-aligned (Next in the far corner).
    const int gap = ClampPositive(style.control_gap);
    const int next_x = layout.control_row.right() - icon_size;
    const int prev_x = next_x - gap - icon_size;
    const int close_x = prev_x - gap - close_w;
    const int icon_y = layout.control_row.y + std::max(0, (row_height - icon_size) / 2);
    layout.next = {next_x, icon_y, icon_size, icon_size};
    layout.prev = {prev_x, icon_y, icon_size, icon_size};
    layout.close = {close_x, layout.control_row.y + std::max(0, (row_height - close_h) / 2), close_w,
                    close_h};

    // Pagination dots, bottom-left, vertically centered to the icon buttons.
    const int dot = ClampPositive(style.dot_diameter);
    layout.dots_x = inner.x;
    layout.dots_y = layout.next.y + std::max(0, (icon_size - dot) / 2);

    // Content slot above the control row.
    const int content_bottom = layout.control_row.y - ClampPositive(style.content_control_gap);
    layout.content = {inner.x, inner.y, inner.width, std::max(0, content_bottom - inner.y)};
    return layout;
}

// Inflate a control's rect into a forgiving touch target, mirroring the global footer's asymmetric
// slop: pad horizontally by slop_x, vertically by slop_y, and extend the bottom to the panel edge so
// slightly-low taps (and the gaps between controls) still register.
UiRect InflateForTouch(const UiRect& rect, int slop_x, int slop_y, int portrait_height)
{
    if (rect.IsEmpty()) {
        return rect;
    }
    const int pad_x = std::max(0, slop_x);
    const int pad_y = std::max(0, slop_y);
    const int left = rect.x - pad_x;
    const int top = rect.y - pad_y;
    const int right = rect.right() + pad_x;
    const int bottom = std::max(rect.bottom() + pad_y, portrait_height);
    return {left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

ButtonIconStyle ControlIconStyle(const CarouselStyle& style, bool disabled)
{
    ButtonIconStyle icon_style = {};
    icon_style.size = ClampPositive(style.icon_button_size);
    icon_style.icon_size = ClampPositive(style.icon_size);
    icon_style.border_thickness = 0;
    icon_style.outline_icon_when_unselected = !disabled;
    if (disabled) {
        icon_style.icon_color = style.disabled_control_color;
    }
    return icon_style;
}

}  // namespace

CarouselControlRects CarouselControlBounds(int portrait_width,
                                           int portrait_height,
                                           const CarouselState& state,
                                           const CarouselStyle& style)
{
    const Layout layout = BuildLayout(portrait_width, portrait_height, state, style);
    return {layout.close, layout.prev, layout.next};
}

UiRect CarouselBounds(int portrait_width, int portrait_height, const CarouselStyle& style)
{
    return BuildLayout(portrait_width, portrait_height, {}, style).region;
}

UiRect CarouselContentBounds(int portrait_width, int portrait_height, const CarouselStyle& style)
{
    return BuildLayout(portrait_width, portrait_height, {}, style).content;
}

bool HitTestCarouselClose(int portrait_width,
                          int portrait_height,
                          const CarouselState& state,
                          const CarouselStyle& style,
                          int x,
                          int y)
{
    if (!state.show_close) {
        return false;
    }
    const Layout layout = BuildLayout(portrait_width, portrait_height, state, style);
    return InflateForTouch(layout.close, style.touch_slop_x, style.touch_slop_y, portrait_height)
        .Contains(x, y);
}

bool HitTestCarouselPrev(int portrait_width,
                         int portrait_height,
                         const CarouselState& state,
                         const CarouselStyle& style,
                         int x,
                         int y)
{
    if (CarouselPrevDisabled(state)) {
        return false;
    }
    const Layout layout = BuildLayout(portrait_width, portrait_height, state, style);
    return InflateForTouch(layout.prev, style.touch_slop_x, style.touch_slop_y, portrait_height)
        .Contains(x, y);
}

bool HitTestCarouselNext(int portrait_width,
                         int portrait_height,
                         const CarouselState& state,
                         const CarouselStyle& style,
                         int x,
                         int y)
{
    if (CarouselNextDisabled(state)) {
        return false;
    }
    const Layout layout = BuildLayout(portrait_width, portrait_height, state, style);
    return InflateForTouch(layout.next, style.touch_slop_x, style.touch_slop_y, portrait_height)
        .Contains(x, y);
}

void DrawCarousel(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  const CarouselState& state,
                  const CarouselStyle& style)
{
    if (framebuffer == nullptr) {
        return;
    }

    const Layout layout = BuildLayout(portrait_width, portrait_height, state, style);

    // Pagination dots.
    const int dot = ClampPositive(style.dot_diameter);
    const int dot_step = dot + ClampPositive(style.dot_gap);
    for (int index = 0; index < state.slide_count; ++index) {
        const UiRect dot_rect = {layout.dots_x + (index * dot_step), layout.dots_y, dot, dot};
        const uint8_t color =
            index == state.active_index ? style.active_dot_color : style.inactive_dot_color;
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                dot_rect, dot / 2, color);
    }

    // Close button (only on the final slide).
    if (state.show_close) {
        DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.close.x, layout.close.y, CloseButtonState(state, style),
                   style.close_button);
    }

    // Prev / Next chevron icon buttons (greyed at the ends).
    const bool prev_disabled = CarouselPrevDisabled(state);
    const ButtonIconState prev_state = {
        .asset = project_assets::GetIcon(EmbeddedIconId::kChevronLeft),
        .selected = !prev_disabled && state.prev_selected,
    };
    DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.prev.x, layout.prev.y, prev_state, ControlIconStyle(style, prev_disabled));

    const bool next_disabled = CarouselNextDisabled(state);
    const ButtonIconState next_state = {
        .asset = project_assets::GetIcon(EmbeddedIconId::kChevronRight),
        .selected = !next_disabled && state.next_selected,
    };
    DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                   layout.next.x, layout.next.y, next_state, ControlIconStyle(style, next_disabled));
}

}  // namespace epaper_ui
