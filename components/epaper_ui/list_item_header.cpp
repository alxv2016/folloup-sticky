#include "epaper_ui/list_item_header.h"

#include <algorithm>
#include <string>

#include "asset_types.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

int ResolveWidth(int origin_x, int portrait_width, const ListItemHeaderStyle& style)
{
    if (style.width > 0) {
        return style.width;
    }
    return std::max(0, portrait_width - origin_x);
}

int ResolveHeight(const ListItemHeaderStyle& style)
{
    return std::max({ClampPositive(style.height), ClampPositive(style.icon_slot_size),
                     LineHeight(style.role)});
}

// Truncate `text` with a trailing ellipsis so it fits `max_width`. Returns empty when even
// the ellipsis will not fit.
std::string FitLabelText(design::TypographyRole role, const std::string& text, int max_width)
{
    if (text.empty() || max_width <= 0) {
        return {};
    }
    if (MeasureText(role, text) <= max_width) {
        return text;
    }
    constexpr const char* kEllipsis = "...";
    if (MeasureText(role, kEllipsis) > max_width) {
        return {};
    }
    for (size_t length = text.size(); length > 0; --length) {
        const std::string candidate = text.substr(0, length) + kEllipsis;
        if (MeasureText(role, candidate) <= max_width) {
            return candidate;
        }
    }
    return kEllipsis;
}

// Invoke `fn(dx, dy)` for every stroke offset in the (2t+1)^2 neighborhood except the center,
// matching followup's outline pass used to knock content out against a filled background.
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

void DrawCenteredIcon(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const UiRect& slot,
                      const EmbeddedImageAsset* asset,
                      uint8_t color,
                      bool outlined,
                      int stroke_thickness,
                      uint8_t outline_color)
{
    if (asset == nullptr || asset->data == nullptr) {
        return;
    }
    const int draw_width = std::min(slot.width, static_cast<int>(asset->width));
    const int draw_height = std::min(slot.height, static_cast<int>(asset->height));
    const int x = slot.x + CenterOffset(slot.width, draw_width);
    const int y = slot.y + CenterOffset(slot.height, draw_height);
    if (outlined) {
        ForEachOutlineOffset(stroke_thickness, [&](int dx, int dy) {
            DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, x + dx, y + dy, asset, outline_color);
        });
    }
    DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width, portrait_height, x, y,
                          asset, color);
}

void DrawOutlinedText(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      int x,
                      int y,
                      const std::string& text,
                      design::TypographyRole role,
                      uint8_t color,
                      bool outlined,
                      int stroke_thickness,
                      uint8_t outline_color)
{
    if (outlined) {
        ForEachOutlineOffset(stroke_thickness, [&](int dx, int dy) {
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               x + dx, y + dy, text, role, outline_color);
        });
    }
    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height, x, y,
                       text, role, color);
}

}  // namespace

UiRect ListItemHeaderBounds(int origin_x,
                            int origin_y,
                            const ListItemHeaderState&,
                            const ListItemHeaderStyle& style)
{
    return {origin_x, origin_y, ResolveWidth(origin_x, 0, style), ResolveHeight(style)};
}

void DrawListItemHeader(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int origin_x,
                        int origin_y,
                        const ListItemHeaderState& state,
                        const ListItemHeaderStyle& style)
{
    const int width = ResolveWidth(origin_x, portrait_width, style);
    const int height = ResolveHeight(style);
    if (width <= 0 || height <= 0) {
        return;
    }

    const UiRect bounds = {origin_x, origin_y, width, height};
    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;
    const uint8_t icon_color = state.selected ? style.selected_icon_color : style.icon_color;
    const uint8_t divider_color =
        state.selected ? style.selected_divider_color : style.divider_color;
    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height, bounds,
                     background_color);

    const bool outline = state.selected && style.selected_content_outlined;
    const int stroke = style.selected_content_stroke_thickness;
    const uint8_t outline_color = style.selected_content_outline_color;

    // Trailing tag pill, right-aligned and vertically centered.
    int content_right = bounds.right();
    int tag_left = bounds.right();
    if (!state.tag_text.empty()) {
        TagState tag_state = {.label_text = state.tag_text, .selected = state.selected};
        const UiRect probe = TagBounds(0, 0, tag_state, style.tag);
        const int tag_x = bounds.right() - probe.width;
        const int tag_y = bounds.y + CenterOffset(bounds.height, probe.height);
        DrawTag(framebuffer, raw_width, raw_height, portrait_width, portrait_height, tag_x, tag_y,
                tag_state, style.tag);
        tag_left = tag_x;
        content_right = tag_x;
    }

    // Trailing status icon (e.g. follow-up pin), just left of the tag pill.
    const int tag_icon_slot_size = ClampPositive(style.tag_icon_slot_size);
    if (state.tag_icon_asset != nullptr && tag_icon_slot_size > 0) {
        const int gap_before_tag =
            (tag_left < bounds.right()) ? ClampPositive(style.tag_icon_gap) : 0;
        const int slot_x = tag_left - gap_before_tag - tag_icon_slot_size;
        const UiRect slot = {slot_x, bounds.y, tag_icon_slot_size, bounds.height};
        DrawCenteredIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height, slot,
                         state.tag_icon_asset, icon_color, outline, stroke, outline_color);
        content_right = slot_x;
    }

    // Leading icon.
    const int icon_slot_size = ClampPositive(style.icon_slot_size);
    int cursor_x = bounds.x;
    if (state.icon_asset != nullptr) {
        const UiRect slot = {cursor_x, bounds.y, icon_slot_size, bounds.height};
        DrawCenteredIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height, slot,
                         state.icon_asset, icon_color, outline, stroke, outline_color);
    }
    cursor_x += icon_slot_size;

    if (content_right <= cursor_x) {
        return;
    }

    const int content_gap = ClampPositive(style.content_gap);
    const int dot_diameter = ClampPositive(style.divider_dot_diameter);
    const bool has_time = !state.time_text.empty();
    const bool has_detail = !state.minute_seconds_text.empty();
    if ((has_time || has_detail) && icon_slot_size > 0) {
        cursor_x += content_gap;
    }

    const int text_y = bounds.y + CenterOffset(bounds.height, LineHeight(style.role));

    std::string fitted_time;
    if (has_time) {
        int time_max_width = std::max(0, content_right - cursor_x);
        if (has_detail) {
            const int detail_width = MeasureText(style.role, state.minute_seconds_text);
            time_max_width =
                std::max(0, time_max_width - detail_width - dot_diameter - (2 * content_gap));
        }
        fitted_time = FitLabelText(style.role, state.time_text, time_max_width);
        if (fitted_time.empty() && !has_detail) {
            fitted_time = FitLabelText(style.role, state.time_text,
                                       std::max(0, content_right - cursor_x));
        }
    }

    if (!fitted_time.empty()) {
        DrawOutlinedText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         cursor_x, text_y, fitted_time, style.role, text_color, outline, stroke,
                         outline_color);
        cursor_x += MeasureText(style.role, fitted_time);
    }

    if (has_detail) {
        if (!fitted_time.empty()) {
            cursor_x += content_gap;
            const UiRect dot = {cursor_x, bounds.y + CenterOffset(bounds.height, dot_diameter),
                                dot_diameter, dot_diameter};
            if (outline) {
                ForEachOutlineOffset(stroke, [&](int dx, int dy) {
                    const UiRect ring = {dot.x + dx, dot.y + dy, dot.width, dot.height};
                    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                            portrait_height, ring, dot_diameter / 2, outline_color);
                });
            }
            FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                    portrait_height, dot, dot_diameter / 2, divider_color);
            cursor_x += dot_diameter + content_gap;
        }
        const std::string fitted_detail =
            FitLabelText(style.role, state.minute_seconds_text, std::max(0, content_right - cursor_x));
        if (!fitted_detail.empty()) {
            DrawOutlinedText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             cursor_x, text_y, fitted_detail, style.role, text_color, outline,
                             stroke, outline_color);
        }
    }
}

}  // namespace epaper_ui
