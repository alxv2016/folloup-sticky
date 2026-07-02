#include "epaper_ui/scroll_container.h"

#include <algorithm>
#include <string>
#include <vector>

#include "asset_types.h"
#include "generated_epaper_icons.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kPanelCornerRadius = 4;

int FocusPadding(const ScrollContainerStyle& style)
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

// Split on hard newlines, wrap each paragraph to max_width, preserving blank lines.
std::vector<std::string> BuildWrappedLines(const std::string& text, design::TypographyRole role,
                                           int max_width)
{
    std::vector<std::string> lines;
    if (max_width <= 0) {
        lines.emplace_back("");
        return lines;
    }
    size_t start = 0;
    while (start <= text.size()) {
        const size_t newline = text.find('\n', start);
        const std::string paragraph = text.substr(
            start, newline == std::string::npos ? std::string::npos : newline - start);
        const std::vector<std::string> wrapped = WrapTextToWidth(role, paragraph, max_width);
        if (wrapped.empty()) {
            lines.emplace_back("");
        } else {
            lines.insert(lines.end(), wrapped.begin(), wrapped.end());
        }
        if (newline == std::string::npos) {
            break;
        }
        start = newline + 1;
    }
    if (lines.empty()) {
        lines.emplace_back("");
    }
    return lines;
}

int LineStride(const ScrollContainerStyle& style)
{
    return LineHeight(style.text_role) + ClampPositive(style.line_gap);
}

int ClampedPercent(const ScrollContainerState& state)
{
    return std::clamp(state.scroll_position_percent, 0, 100);
}

UiRect ScrollbarBounds(const UiRect& panel, const ScrollContainerStyle& style)
{
    const UiRect inner = InsetRect(panel, ClampPositive(style.panel_border_thickness));
    const int scrollbar_width = ClampPositive(style.scrollbar_width);
    return {inner.right() - scrollbar_width, inner.y, scrollbar_width, std::max(0, inner.height)};
}

UiRect ContentViewport(const UiRect& panel, const ScrollContainerStyle& style)
{
    const UiRect inner = InsetRect(panel, ClampPositive(style.panel_border_thickness));
    const UiRect scrollbar = ScrollbarBounds(panel, style);
    const int padding = ClampPositive(style.content_padding);
    const int scrollbar_gap = scrollbar.width > 0 ? ClampPositive(style.scrollbar_gap) : 0;
    return {inner.x + padding, inner.y + padding,
            std::max(0, scrollbar.x - scrollbar_gap - (inner.x + padding)),
            std::max(0, inner.height - (2 * padding))};
}

int VisibleLineCapacity(const UiRect& viewport, const ScrollContainerStyle& style)
{
    const int stride = LineStride(style);
    if (viewport.height <= 0 || stride <= 0) {
        return 0;
    }
    return std::max(1, (viewport.height + ClampPositive(style.line_gap)) / stride);
}

}  // namespace

UiRect ScrollContainerPanelBounds(int origin_x, int origin_y, const ScrollContainerStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.panel_height)};
}

UiRect ScrollContainerContentViewportBounds(int origin_x,
                                            int origin_y,
                                            const ScrollContainerStyle& style)
{
    return ContentViewport(ScrollContainerPanelBounds(origin_x, origin_y, style), style);
}

bool ScrollContainerHasOverflow(int origin_x,
                                int origin_y,
                                const ScrollContainerState& state,
                                const ScrollContainerStyle& style)
{
    if (state.content_text.empty()) {
        return false;
    }
    const UiRect panel = ScrollContainerPanelBounds(origin_x, origin_y, style);
    const UiRect viewport = ContentViewport(panel, style);
    const int total = static_cast<int>(
        BuildWrappedLines(state.content_text, style.text_role, viewport.width).size());
    return total > VisibleLineCapacity(viewport, style);
}

void DrawScrollContainer(uint8_t* framebuffer,
                         int raw_width,
                         int raw_height,
                         int portrait_width,
                         int portrait_height,
                         int origin_x,
                         int origin_y,
                         const ScrollContainerState& state,
                         const ScrollContainerStyle& style)
{
    if (style.panel_height <= 0 || style.scrollbar_width <= 0) {
        return;
    }
    const UiRect panel = ScrollContainerPanelBounds(origin_x, origin_y, style);
    if (panel.IsEmpty()) {
        return;
    }

    if (state.focus_ring_visible && (state.focused || state.active)) {
        const uint8_t ring_color =
            state.active ? style.active_focus_ring_color : style.inactive_focus_ring_color;
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(panel, FocusPadding(style)),
                                kPanelCornerRadius + FocusPadding(style), ring_color);
        FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                                ExpandRect(panel, ClampPositive(style.focus_gap)),
                                kPanelCornerRadius + ClampPositive(style.focus_gap),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            panel, kPanelCornerRadius, style.panel_background_color);
    if (style.panel_border_thickness > 0) {
        DrawRoundedPortraitBorder(framebuffer, raw_width, raw_height, portrait_width,
                                  portrait_height, panel, kPanelCornerRadius,
                                  style.panel_border_thickness, style.panel_border_color);
    }

    const UiRect viewport = ContentViewport(panel, style);
    if (state.content_text.empty()) {
        const EmbeddedImageAsset* star = &epaper_icons::kStar;
        const int label_height =
            state.empty_state_message.empty() ? 0 : LineHeight(style.empty_state_role);
        const int icon_size = ClampPositive(style.empty_state_icon_size);
        const int stack_height =
            icon_size + (icon_size > 0 && label_height > 0 ? ClampPositive(style.empty_state_gap) : 0) +
            label_height;
        int cursor_y = viewport.y + CenterOffset(viewport.height, stack_height);

        if (star != nullptr && icon_size > 0) {
            const UiRect icon_dest = {viewport.x + CenterOffset(viewport.width, icon_size), cursor_y,
                                      icon_size, icon_size};
            DrawScaledPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                        portrait_height, icon_dest, star,
                                        style.empty_state_icon_color);
            cursor_y += icon_size + (label_height > 0 ? ClampPositive(style.empty_state_gap) : 0);
        }
        if (!state.empty_state_message.empty()) {
            const int text_width = MeasureText(style.empty_state_role, state.empty_state_message);
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               viewport.x + CenterOffset(viewport.width, text_width), cursor_y,
                               state.empty_state_message, style.empty_state_role,
                               style.empty_state_text_color);
        }
    } else {
        const std::vector<std::string> lines =
            BuildWrappedLines(state.content_text, style.text_role, viewport.width);
        const int stride = LineStride(style);
        const int line_height = LineHeight(style.text_role);
        const int capacity = VisibleLineCapacity(viewport, style);
        const int max_offset = std::max(0, static_cast<int>(lines.size()) - capacity);
        const int first_visible =
            max_offset > 0 ? ((max_offset * ClampedPercent(state)) + 50) / 100 : 0;
        const int end_visible = std::min(static_cast<int>(lines.size()), first_visible + capacity);

        int y = viewport.y;
        for (int index = first_visible; index < end_visible; ++index) {
            if (y + line_height > viewport.bottom()) {
                break;
            }
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               viewport.x, y, lines[static_cast<size_t>(index)], style.text_role,
                               style.text_color);
            y += stride;
        }
    }

    // Scrollbar: track (bordered) + thumb sized/positioned to the scroll fraction.
    const UiRect scrollbar = ScrollbarBounds(panel, style);
    const bool has_overflow = ScrollContainerHasOverflow(origin_x, origin_y, state, style);
    const bool scrollbar_focused = state.active;
    const uint8_t thumb_color =
        scrollbar_focused
            ? (has_overflow ? style.scrollbar_focused_color : style.scrollbar_disabled_focused_color)
            : style.scrollbar_inactive_color;
    if (style.scrollbar_border_thickness > 0) {
        DrawPortraitBorder(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           scrollbar, style.scrollbar_border_thickness, style.scrollbar_border_color);
    }
    const UiRect track_inner = InsetRect(scrollbar, ClampPositive(style.scrollbar_border_thickness));
    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     track_inner, style.scrollbar_track_color);

    if (!track_inner.IsEmpty()) {
        const UiRect viewport_for_thumb = viewport;
        const int total = state.content_text.empty()
                              ? 0
                              : static_cast<int>(BuildWrappedLines(state.content_text,
                                                                   style.text_role,
                                                                   viewport_for_thumb.width)
                                                     .size());
        const int capacity = VisibleLineCapacity(viewport_for_thumb, style);
        UiRect thumb = track_inner;
        if (total > 0 && capacity > 0) {
            const int thumb_height = std::clamp(
                (track_inner.height * capacity + (total / 2)) / total,
                std::min(track_inner.height, ClampPositive(style.min_thumb_height)),
                track_inner.height);
            const int max_offset = std::max(0, total - capacity);
            if (max_offset <= 0) {
                thumb = track_inner;
            } else {
                const int travel = std::max(0, track_inner.height - thumb_height);
                thumb = {track_inner.x, track_inner.y + ((travel * ClampedPercent(state)) + 50) / 100,
                         track_inner.width, thumb_height};
            }
        }
        FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height, thumb,
                         thumb_color);
    }
}

}  // namespace epaper_ui
