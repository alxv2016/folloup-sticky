#include "epaper_ui/segment_control.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kControlCornerRadius = 4;

int FocusPadding(const SegmentControlStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

UiRect InsetRect(const UiRect& rect, int inset)
{
    const int safe = std::max(0, inset);
    return {rect.x + safe, rect.y + safe, std::max(0, rect.width - (2 * safe)),
            std::max(0, rect.height - (2 * safe))};
}

UiRect ExpandRect(const UiRect& rect, int amount)
{
    return {rect.x - amount, rect.y - amount, rect.width + (2 * amount), rect.height + (2 * amount)};
}

int ClampSelectedIndex(const SegmentControlState& state, int visible_count)
{
    if (state.selected_index < 0 || state.selected_index >= visible_count) {
        return 0;
    }
    return state.selected_index;
}

}  // namespace

int SegmentControlVisibleCount(const SegmentControlState& state)
{
    return std::clamp(state.segment_count, kSegmentControlMinSegmentCount,
                      kSegmentControlMaxSegmentCount);
}

UiRect SegmentControlBounds(int origin_x, int origin_y, const SegmentControlStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.field_height)};
}

UiRect SegmentControlSegmentBounds(int origin_x,
                                   int origin_y,
                                   const SegmentControlState& state,
                                   const SegmentControlStyle& style,
                                   int index)
{
    const UiRect field = SegmentControlBounds(origin_x, origin_y, style);
    const int segment_count = SegmentControlVisibleCount(state);
    if (field.IsEmpty() || index < 0 || index >= segment_count) {
        return {field.x, field.y, 0, 0};
    }
    const int gap = ClampPositive(style.segment_gap);
    const UiRect inner = InsetRect(field, ClampPositive(style.internal_padding));
    const int total_gap = gap * std::max(0, segment_count - 1);
    const int available_width = std::max(0, inner.width - total_gap);
    const int base_width = segment_count > 0 ? available_width / segment_count : 0;
    const int remainder = segment_count > 0 ? available_width % segment_count : 0;

    int x = inner.x;
    for (int current = 0; current < index; ++current) {
        x += base_width + (current < remainder ? 1 : 0) + gap;
    }
    return {x, inner.y, base_width + (index < remainder ? 1 : 0), std::max(0, inner.height)};
}

bool HitTestSegmentControlSegment(int origin_x,
                                  int origin_y,
                                  const SegmentControlState& state,
                                  const SegmentControlStyle& style,
                                  int x,
                                  int y,
                                  int* index)
{
    if (index != nullptr) {
        *index = kSegmentControlNoSelection;
    }
    const int segment_count = SegmentControlVisibleCount(state);
    for (int candidate = 0; candidate < segment_count; ++candidate) {
        const UiRect bounds =
            SegmentControlSegmentBounds(origin_x, origin_y, state, style, candidate);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (index != nullptr) {
                *index = candidate;
            }
            return true;
        }
    }
    return false;
}

void DrawSegmentControl(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int origin_x,
                        int origin_y,
                        const SegmentControlState& state,
                        const SegmentControlStyle& style)
{
    const UiRect field = SegmentControlBounds(origin_x, origin_y, style);
    if (field.IsEmpty()) {
        return;
    }

    if (state.focus_ring_visible && (state.focused || state.active)) {
        const uint8_t ring_color =
            state.active ? style.active_focus_ring_color : style.inactive_focus_ring_color;
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(field, FocusPadding(style)),
                                kControlCornerRadius + FocusPadding(style), ring_color);
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(field, ClampPositive(style.focus_gap)),
                                kControlCornerRadius + ClampPositive(style.focus_gap),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            field, kControlCornerRadius, style.background_color);
    if (style.border_thickness > 0) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, field, kControlCornerRadius,
                                  style.border_thickness, style.border_color);
    }

    const int segment_count = SegmentControlVisibleCount(state);
    const int selected_index = ClampSelectedIndex(state, segment_count);
    for (int index = 0; index < segment_count; ++index) {
        const UiRect segment =
            SegmentControlSegmentBounds(origin_x, origin_y, state, style, index);
        const bool selected = index == selected_index;
        ButtonState button_state = {
            .label_text = state.labels[static_cast<size_t>(index)],
            .selected = selected,
        };
        ButtonStyle button_style = style.button;
        button_style.width = segment.width;
        button_style.min_width = 0;
        button_style.height = segment.height;
        button_style.selected_background_color = state.active
                                                     ? style.active_selected_background_color
                                                     : style.selected_background_color;
        button_style.border_thickness = selected ? style.button.border_thickness : 0;
        DrawButton(framebuffer, raw_width, raw_height, portrait_width, portrait_height, segment.x,
                   segment.y, button_state, button_style);
    }
}

}  // namespace epaper_ui
