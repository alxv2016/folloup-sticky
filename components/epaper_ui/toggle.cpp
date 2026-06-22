#include "epaper_ui/toggle.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

bool IsFocused(ToggleVisualState state)
{
    return state == ToggleVisualState::kFocusOff || state == ToggleVisualState::kFocusOn;
}

bool IsOn(ToggleVisualState state)
{
    return state == ToggleVisualState::kOn || state == ToggleVisualState::kFocusOn;
}

int FocusPadding(const ToggleStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

UiRect TrackBounds(int origin_x, int origin_y, const ToggleStyle& style)
{
    const int focus_inset = FocusPadding(style);
    return {origin_x + focus_inset,
            origin_y + focus_inset,
            ClampPositive(style.width),
            ClampPositive(style.height)};
}

UiRect InsetRect(const UiRect& rect, int inset)
{
    const int safe_inset = std::max(0, inset);
    return {rect.x + safe_inset,
            rect.y + safe_inset,
            std::max(0, rect.width - (2 * safe_inset)),
            std::max(0, rect.height - (2 * safe_inset))};
}

void FillCircle(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int center_x,
                int center_y,
                int radius,
                uint8_t tone)
{
    if (radius <= 0) {
        return;
    }

    const int radius_sq = radius * radius;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if ((dx * dx) + (dy * dy) > radius_sq) {
                continue;
            }
            const int px = center_x + dx;
            const int py = center_y + dy;
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

void FillRing(uint8_t* framebuffer,
              int raw_width,
              int raw_height,
              int portrait_width,
              int portrait_height,
              int center_x,
              int center_y,
              int outer_radius,
              int inner_radius,
              uint8_t tone)
{
    if (outer_radius <= 0 || outer_radius <= inner_radius) {
        return;
    }

    const int outer_sq = outer_radius * outer_radius;
    const int inner = std::max(0, inner_radius);
    const int inner_sq = inner * inner;
    for (int dy = -outer_radius; dy <= outer_radius; ++dy) {
        for (int dx = -outer_radius; dx <= outer_radius; ++dx) {
            const int distance_sq = (dx * dx) + (dy * dy);
            if (distance_sq > outer_sq || distance_sq < inner_sq) {
                continue;
            }

            const int px = center_x + dx;
            const int py = center_y + dy;
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

}  // namespace

UiRect ToggleBounds(int origin_x, int origin_y, const ToggleStyle& style)
{
    const int focus_padding = FocusPadding(style);
    return {origin_x,
            origin_y,
            ClampPositive(style.width) + (2 * focus_padding),
            ClampPositive(style.height) + (2 * focus_padding)};
}

void DrawToggle(uint8_t* framebuffer,
                int raw_width,
                int raw_height,
                int portrait_width,
                int portrait_height,
                int origin_x,
                int origin_y,
                ToggleVisualState state,
                const ToggleStyle& style)
{
    const UiRect bounds = ToggleBounds(origin_x, origin_y, style);
    const UiRect track_bounds = TrackBounds(origin_x, origin_y, style);
    if (bounds.IsEmpty() || track_bounds.IsEmpty()) {
        return;
    }

    const bool focused = IsFocused(state);
    const bool on = IsOn(state);

    if (focused) {
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                bounds,
                                ClampPositive(bounds.height / 2),
                                style.focus_ring_color);
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                InsetRect(bounds, ClampPositive(style.focus_ring_thickness)),
                                ClampPositive(std::max(0, bounds.height / 2 - style.focus_ring_thickness)),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            track_bounds,
                            ClampPositive(track_bounds.height / 2),
                            style.track_border_color);
    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            InsetRect(track_bounds, ClampPositive(style.track_border_thickness)),
                            ClampPositive(
                                std::max(0, track_bounds.height / 2 - style.track_border_thickness)),
                            style.track_color);

    const int thumb_size = ClampPositive(style.thumb_size);
    const int thumb_radius = thumb_size / 2;
    const int inset = ClampPositive(style.thumb_inset);
    const int thumb_x =
        on ? track_bounds.right() - inset - thumb_size : track_bounds.x + inset;
    const int thumb_center_x = thumb_x + (thumb_size / 2);
    const int thumb_center_y = track_bounds.y + (track_bounds.height / 2);

    if (on) {
        FillCircle(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   thumb_center_x,
                   thumb_center_y,
                   thumb_radius,
                   style.thumb_on_fill_color);
        return;
    }

    FillCircle(framebuffer,
               raw_width,
               raw_height,
               portrait_width,
               portrait_height,
               thumb_center_x,
               thumb_center_y,
               thumb_radius,
               style.thumb_off_fill_color);
    FillRing(framebuffer,
             raw_width,
             raw_height,
             portrait_width,
             portrait_height,
             thumb_center_x,
             thumb_center_y,
             thumb_radius,
             std::max(0, thumb_radius - ClampPositive(style.thumb_border_thickness)),
             style.thumb_off_border_color);
}

}  // namespace epaper_ui
