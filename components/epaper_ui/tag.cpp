#include "epaper_ui/tag.h"

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kTagCornerRadius = 4;

// Text needs a white knockout halo only when it sits on a light surface; over a solid
// black pill the plain white glyphs read fine on their own.
bool ShouldOutlineText(uint8_t background_color)
{
    return background_color == design::color::kGrayLight ||
           background_color == design::status_bar::kBackgroundColor;
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
}

}  // namespace

UiRect TagBounds(int origin_x, int origin_y, const TagState& state, const TagStyle& style)
{
    if (!TagIsValid(state)) {
        return {origin_x, origin_y, 0, 0};
    }
    const int text_width = MeasureText(style.role, state.label_text);
    const int text_height = LineHeight(style.role);
    const int horizontal_padding = ClampPositive(style.horizontal_padding);
    const int vertical_padding = ClampPositive(style.vertical_padding);
    return {
        origin_x,
        origin_y,
        text_width + (2 * horizontal_padding),
        text_height + (2 * vertical_padding),
    };
}

bool TagIsValid(const TagState& state)
{
    return !state.label_text.empty();
}

void DrawTag(uint8_t* framebuffer,
             int raw_width,
             int raw_height,
             int portrait_width,
             int portrait_height,
             int origin_x,
             int origin_y,
             const TagState& state,
             const TagStyle& style)
{
    if (!TagIsValid(state)) {
        return;
    }

    const UiRect bounds = TagBounds(origin_x, origin_y, state, style);
    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;

    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            bounds, kTagCornerRadius, background_color);
    if (style.border_thickness > 0) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, bounds, kTagCornerRadius, style.border_thickness,
                                  style.border_color);
    }

    const int text_x = bounds.x + ClampPositive(style.horizontal_padding);
    const int text_y = bounds.y + CenterOffset(bounds.height, LineHeight(style.role));
    const auto draw_glyphs = [&](int x, int y, uint8_t tone) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height, x, y,
                           state.label_text, style.role, tone);
    };
    if (ShouldOutlineText(background_color)) {
        DrawOutlinedText(text_x, text_y, style.stroke_thickness, draw_glyphs);
    }
    draw_glyphs(text_x, text_y, text_color);
}

}  // namespace epaper_ui
