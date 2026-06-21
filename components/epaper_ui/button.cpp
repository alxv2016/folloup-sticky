#include "epaper_ui/button.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

int ResolveWidth(const ButtonState& state, const ButtonStyle& style)
{
    const int padding = ClampPositive(style.horizontal_padding);
    const int text_width = state.label_text.empty() ? 0 : MeasureText(style.role, state.label_text);
    const int content_width = text_width + (2 * padding);
    const int requested_width = style.width > 0 ? style.width : content_width;
    return std::max(ClampPositive(style.min_width), requested_width);
}

uint8_t ResolveBackgroundColor(const ButtonState& state, const ButtonStyle& style)
{
    if (state.selected) {
        return style.selected_background_color;
    }
    if (style.variant == ButtonVariant::kPrimary) {
        return design::vibe_card::kFooterButtonBackgroundColor;
    }
    return style.background_color;
}

template <typename DrawFn>
void DrawOutlinedText(int origin_x, int origin_y, int stroke_thickness, DrawFn&& draw_fn)
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

UiRect ButtonBounds(int origin_x, int origin_y, const ButtonState& state, const ButtonStyle& style)
{
    return {origin_x, origin_y, ResolveWidth(state, style), ClampPositive(style.height)};
}

void DrawButton(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int origin_x,
                int origin_y,
                const ButtonState& state,
                const ButtonStyle& style)
{
    const UiRect bounds = ButtonBounds(origin_x, origin_y, state, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const uint8_t background_color = ResolveBackgroundColor(state, style);
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

    if (state.label_text.empty()) {
        return;
    }

    const int text_width = MeasureText(style.role, state.label_text);
    const int text_x = style.center_label
                           ? bounds.x + std::max(0, (bounds.width - text_width) / 2)
                           : bounds.x + ClampPositive(style.horizontal_padding);
    const int text_y =
        bounds.y + CenterOffset(bounds.height, LineHeight(style.role)) + design::button::kLabelYOffset;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;

    if (state.selected) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           text_x,
                           text_y,
                           state.label_text,
                           style.role,
                           text_color);
        return;
    }

    DrawOutlinedText(text_x,
                     text_y,
                     style.stroke_thickness,
                     [&](int draw_x, int draw_y, uint8_t tone) {
                         DrawTypographyText(framebuffer,
                                            raw_width,
                                            raw_height,
                                            portrait_width,
                                            portrait_height,
                                            draw_x,
                                            draw_y,
                                            state.label_text,
                                            style.role,
                                            tone);
                     });
}

}  // namespace epaper_ui
