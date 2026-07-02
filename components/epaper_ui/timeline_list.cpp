#include "epaper_ui/timeline_list.h"

#include <algorithm>
#include <string>
#include <utility>

#include "asset_types.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

int ClampIndex(int index, int count)
{
    return index >= 0 && index < count ? index : -1;
}

int ResolveWidth(int canvas_width, int origin_x, const TimelineListStyle& style)
{
    if (style.width > 0) {
        return style.width;
    }
    return std::max(0, canvas_width - origin_x);
}

int ResolveHeight(int canvas_height, int origin_y, const TimelineListStyle& style)
{
    if (style.height > 0) {
        return style.height;
    }
    return std::max(0, canvas_height - origin_y);
}

int ScrollbarReservedWidth(const TimelineListStyle& style)
{
    const int scrollbar_width = ClampPositive(style.scrollbar_width);
    if (scrollbar_width <= 0) {
        return 0;
    }
    return scrollbar_width + ClampPositive(style.scrollbar_gap);
}

int ContentWidth(int total_width, const TimelineListStyle& style)
{
    return std::max(0, total_width - ScrollbarReservedWidth(style));
}

UiRect GroupScrollbarBounds(int right_edge, int top, int height, const TimelineListStyle& style)
{
    const int scrollbar_width = ClampPositive(style.scrollbar_width);
    if (height <= 0 || scrollbar_width <= 0) {
        return {right_edge, top, 0, 0};
    }
    return {right_edge - scrollbar_width, top, scrollbar_width, height};
}

UiRect ExpandRect(const UiRect& rect, int pad)
{
    return {rect.x - pad, rect.y - pad, rect.width + (2 * pad), rect.height + (2 * pad)};
}

UiRect InsetRect(const UiRect& rect, int inset)
{
    return {rect.x + inset, rect.y + inset, std::max(0, rect.width - (2 * inset)),
            std::max(0, rect.height - (2 * inset))};
}

int FocusPadding(const TimelineListStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

int LabelCornerRadius(const TimelineListStyle& style)
{
    return ClampPositive(style.label_corner_radius);
}

int MaxGroupBodyHeight(int timeline_height, int label_height, const TimelineListStyle& style)
{
    const int percent = std::clamp(style.max_group_height_percent, 0, 100);
    const int max_group_height = (timeline_height * percent) / 100;
    return std::max(0, max_group_height - label_height);
}

int MeasureItemHeight(const ListItemState& item_state, int width, const TimelineListStyle& style)
{
    if (width <= 0) {
        return 0;
    }
    ListItemStyle item_style = style.item;
    item_style.width = width;
    return std::max(1, ListItemBounds(width, 0, 0, item_state, item_style).height);
}

int StickyFooterHeight(const TimelineListStyle& style)
{
    return LineHeight(style.sticky_footer_role) +
           (2 * ClampPositive(style.sticky_footer_vertical_padding));
}

UiRect MeasureLabelChip(int origin_x, int origin_y, const std::string& label_text,
                        const TimelineListStyle& style)
{
    if (label_text.empty()) {
        return {origin_x, origin_y, 0, 0};
    }
    const int horizontal_padding = ClampPositive(style.label_horizontal_padding);
    const int vertical_padding = ClampPositive(style.label_vertical_padding);
    return {origin_x, origin_y,
            MeasureText(style.label_role, label_text) + (2 * horizontal_padding),
            LineHeight(style.label_role) + (2 * vertical_padding)};
}

UiRect MeasureLabelOuterBounds(int origin_x, int origin_y, const std::string& label_text,
                               const TimelineListStyle& style)
{
    const UiRect chip = MeasureLabelChip(origin_x, origin_y, label_text, style);
    const int border = ClampPositive(style.label_border_thickness);
    return {chip.x - border, chip.y - border, chip.width + (2 * border), chip.height + (2 * border)};
}

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

std::string BuildStickyFooterText(const TimelineListState& state, int group_index, int item_count)
{
    if (item_count <= 0) {
        return {};
    }
    const std::string label = state.item_label_plural.empty() ? "Notes" : state.item_label_plural;
    const bool active_group =
        group_index == ClampIndex(state.active_group_index, static_cast<int>(state.groups.size()));
    const int selected_item_index = ClampIndex(state.selected_item_index, item_count);
    if (active_group && selected_item_index >= 0) {
        return std::to_string(selected_item_index + 1) + "/" + std::to_string(item_count) + " " +
               label;
    }
    return std::to_string(item_count) + " " + label;
}

int CenterX(int container_left, int container_width, int item_width)
{
    return container_left + std::max(0, (container_width - item_width) / 2);
}

int CenterY(int container_top, int container_height, int item_height)
{
    return container_top + std::max(0, (container_height - item_height) / 2);
}

bool HasActiveList(const TimelineListState& state)
{
    return ClampIndex(state.active_group_index, static_cast<int>(state.groups.size())) >= 0;
}

uint8_t ResolveFocusRingColor(const TimelineListState& state, const TimelineListStyle& style)
{
    return HasActiveList(state) ? style.active_focus_ring_color : style.inactive_focus_ring_color;
}

int ExpandedGroupIndex(const TimelineListState& state)
{
    const int group_count = static_cast<int>(state.groups.size());
    const int active_group_index = ClampIndex(state.active_group_index, group_count);
    if (active_group_index >= 0) {
        return active_group_index;
    }
    const int focused_group_index = ClampIndex(state.focused_group_index, group_count);
    if (focused_group_index >= 0) {
        return focused_group_index;
    }
    const int visible_group_index = ClampIndex(state.visible_group_index, group_count);
    if (visible_group_index >= 0) {
        return visible_group_index;
    }
    return group_count > 0 ? 0 : -1;
}

std::pair<int, int> VisibleItemRange(const TimelineGroupState& group, int width,
                                     int available_height, int selected_item_index,
                                     bool list_active, const TimelineListStyle& style)
{
    const int item_count = static_cast<int>(group.items.size());
    if (item_count <= 0 || width <= 0 || available_height <= 0) {
        return {0, 0};
    }

    auto fit_range_from = [&](int start_index) {
        int end_index = start_index;
        int used_height = 0;
        while (end_index < item_count) {
            const int item_height =
                MeasureItemHeight(group.items[static_cast<size_t>(end_index)], width, style);
            if (used_height + item_height > available_height) {
                break;
            }
            used_height += item_height;
            ++end_index;
            if (used_height >= available_height) {
                break;
            }
        }
        return std::pair<int, int>{start_index, end_index};
    };

    if (!list_active) {
        return fit_range_from(0);
    }

    const int selected_index = ClampIndex(selected_item_index, item_count);
    if (selected_index < 0) {
        return fit_range_from(0);
    }

    int first_visible = selected_index;
    while (first_visible > 0) {
        const auto candidate = fit_range_from(first_visible - 1);
        if (selected_index < candidate.first || selected_index >= candidate.second) {
            break;
        }
        first_visible = candidate.first;
    }
    return fit_range_from(first_visible);
}

struct TimelineGroupViewportLayout {
    int line_x = 0;
    int item_x = 0;
    int item_width = 0;
    int available_body_height = 0;
    int available_items_height = 0;
    int sticky_footer_height = 0;
    std::string sticky_footer_text = {};
    int first_visible_item = 0;
    int end_visible_item = 0;
    bool has_scrollbar = false;
};

TimelineGroupViewportLayout BuildGroupViewportLayout(const TimelineListState& state,
                                                     const TimelineGroupState& group,
                                                     int group_index, int origin_x, int origin_y,
                                                     int total_width, int viewport_bottom,
                                                     int timeline_height, int label_height,
                                                     bool group_active,
                                                     const TimelineListStyle& style)
{
    TimelineGroupViewportLayout layout = {};
    layout.line_x = origin_x + ClampPositive(style.line_indent);
    layout.item_x = layout.line_x + ClampPositive(style.item_indent);

    const int full_item_width = std::max(0, (origin_x + total_width) - layout.item_x);
    layout.available_body_height = std::min(std::max(0, viewport_bottom - origin_y),
                                            MaxGroupBodyHeight(timeline_height, label_height, style));

    const std::string base_sticky_footer_text =
        BuildStickyFooterText(state, group_index, static_cast<int>(group.items.size()));

    auto solve = [&](int candidate_width) {
        TimelineGroupViewportLayout candidate = layout;
        candidate.item_width = candidate_width;
        candidate.sticky_footer_text = base_sticky_footer_text;
        candidate.sticky_footer_height =
            candidate.sticky_footer_text.empty() ? 0 : StickyFooterHeight(style);
        candidate.available_items_height =
            std::max(0, candidate.available_body_height - candidate.sticky_footer_height);
        std::tie(candidate.first_visible_item, candidate.end_visible_item) =
            VisibleItemRange(group, candidate.item_width, candidate.available_items_height,
                             state.selected_item_index, group_active, style);
        if (candidate.end_visible_item <= candidate.first_visible_item &&
            candidate.sticky_footer_height > 0) {
            candidate.sticky_footer_text.clear();
            candidate.sticky_footer_height = 0;
            candidate.available_items_height = candidate.available_body_height;
            std::tie(candidate.first_visible_item, candidate.end_visible_item) =
                VisibleItemRange(group, candidate.item_width, candidate.available_items_height,
                                 state.selected_item_index, group_active, style);
        }
        return candidate;
    };

    layout = solve(full_item_width);
    const bool overflow = layout.first_visible_item > 0 ||
                          layout.end_visible_item < static_cast<int>(group.items.size());
    if (overflow) {
        layout = solve(ContentWidth(full_item_width, style));
        layout.has_scrollbar = layout.first_visible_item > 0 ||
                               layout.end_visible_item < static_cast<int>(group.items.size());
    }
    return layout;
}

struct VisibleGroupRangeResult {
    int first = 0;
    int end = 0;
    bool target_group_has_visible_body = false;
};

VisibleGroupRangeResult VisibleGroupRange(int canvas_width, int canvas_height,
                                          const TimelineListState& state, int origin_x, int origin_y,
                                          const TimelineListStyle& style)
{
    const int group_count = static_cast<int>(state.groups.size());
    if (group_count <= 0) {
        return {0, 0};
    }
    const int max_height = ResolveHeight(canvas_height, origin_y, style);
    if (max_height <= 0) {
        return {0, 0};
    }
    const int width = ResolveWidth(canvas_width, origin_x, style);
    const bool list_active = HasActiveList(state);

    int anchor = ClampIndex(state.visible_group_index, group_count);
    if (anchor < 0) {
        anchor = ExpandedGroupIndex(state);
    }
    if (anchor < 0) {
        anchor = 0;
    }
    const int target_group_index = ExpandedGroupIndex(state);

    auto fit_range_from = [&](int start_index) {
        start_index = std::clamp(start_index, 0, group_count - 1);
        const int viewport_bottom = origin_y + max_height;
        const int group_gap = ClampPositive(style.group_gap);
        int cursor_y = origin_y;
        int end_index = start_index;
        bool target_group_has_visible_body = false;

        for (int group_index = start_index; group_index < group_count; ++group_index) {
            if (cursor_y >= viewport_bottom) {
                break;
            }
            const TimelineGroupState& group = state.groups[static_cast<size_t>(group_index)];
            const UiRect label_outer =
                MeasureLabelOuterBounds(origin_x, cursor_y, group.label_text, style);
            if (label_outer.IsEmpty() || label_outer.bottom() > viewport_bottom) {
                break;
            }
            cursor_y = label_outer.bottom();

            const TimelineGroupViewportLayout body_layout = BuildGroupViewportLayout(
                state, group, group_index, origin_x, cursor_y, width, viewport_bottom, max_height,
                label_outer.height, list_active && state.active_group_index == group_index, style);

            if (group.items.empty()) {
                const int body_height = std::max(0, body_layout.available_body_height);
                if (body_height <= 0) {
                    break;
                }
                if (group_index == target_group_index) {
                    target_group_has_visible_body = true;
                }
                cursor_y += body_height;
                end_index = group_index + 1;
                break;
            }

            const int first_visible_item = body_layout.first_visible_item;
            const int end_visible_item = body_layout.end_visible_item;
            const int sticky_footer_height = body_layout.sticky_footer_height;
            if (end_visible_item <= first_visible_item) {
                break;
            }

            const int group_body_bottom = std::min(
                viewport_bottom, cursor_y + MaxGroupBodyHeight(max_height, label_outer.height, style));
            int item_cursor_y = cursor_y;
            for (int item_index = first_visible_item; item_index < end_visible_item; ++item_index) {
                const int item_height = MeasureItemHeight(
                    group.items[static_cast<size_t>(item_index)], body_layout.item_width, style);
                if (item_cursor_y + item_height > group_body_bottom - sticky_footer_height) {
                    break;
                }
                item_cursor_y += item_height;
                if (item_cursor_y >= group_body_bottom - sticky_footer_height) {
                    break;
                }
            }

            const int rendered_body_height = std::max(0, item_cursor_y - cursor_y);
            const int line_height = rendered_body_height + sticky_footer_height;
            if (line_height <= 0) {
                break;
            }
            if (group_index == target_group_index && rendered_body_height > 0) {
                target_group_has_visible_body = true;
            }
            cursor_y += line_height;
            end_index = group_index + 1;
            if (group_index < group_count - 1) {
                cursor_y += group_gap;
            }
        }
        return VisibleGroupRangeResult{start_index, end_index, target_group_has_visible_body};
    };

    auto visible_range = fit_range_from(anchor);
    if (target_group_index < 0 ||
        ((target_group_index >= visible_range.first && target_group_index < visible_range.end) &&
         visible_range.target_group_has_visible_body)) {
        return visible_range;
    }

    if (target_group_index < visible_range.first) {
        int first_visible = target_group_index;
        visible_range = fit_range_from(first_visible);
        while (first_visible > 0) {
            const auto candidate = fit_range_from(first_visible - 1);
            if (target_group_index < candidate.first || target_group_index >= candidate.end ||
                !candidate.target_group_has_visible_body) {
                break;
            }
            first_visible = candidate.first;
            visible_range = candidate;
        }
        return visible_range;
    }

    int first_visible = visible_range.first;
    while ((target_group_index >= visible_range.end ||
            !visible_range.target_group_has_visible_body) &&
           first_visible < group_count - 1) {
        ++first_visible;
        visible_range = fit_range_from(first_visible);
    }
    return visible_range;
}

UiRect GroupScrollThumbBounds(const UiRect& scrollbar, int total_items, int first_visible_item,
                              int end_visible_item, const TimelineListStyle& style)
{
    const int border = ClampPositive(style.scrollbar_border_thickness);
    const UiRect track_inner = InsetRect(scrollbar, border);
    if (track_inner.IsEmpty()) {
        return {scrollbar.x, scrollbar.y, 0, 0};
    }
    const int visible_items = std::max(0, end_visible_item - first_visible_item);
    if (total_items <= 0 || visible_items <= 0) {
        return track_inner;
    }
    const int thumb_height = std::clamp(
        (track_inner.height * visible_items + (total_items / 2)) / total_items,
        std::min(track_inner.height, ClampPositive(style.min_thumb_height)), track_inner.height);
    const int max_offset = std::max(0, total_items - visible_items);
    if (max_offset <= 0) {
        return track_inner;
    }
    const int travel = std::max(0, track_inner.height - thumb_height);
    const int thumb_y =
        track_inner.y + ((travel * first_visible_item) + (max_offset / 2)) / max_offset;
    return {track_inner.x, thumb_y, track_inner.width, thumb_height};
}

// A fully resolved, visible group ready to draw or hit-test. Mirrors the layout walk in followup's
// TimelineList::Draw so drawing and touch resolution never diverge.
struct RenderItem {
    int item_index = 0;
    UiRect bounds = {};
    bool selected = false;
    bool last = false;
};

struct RenderGroup {
    int group_index = 0;
    bool focused = false;
    bool active = false;
    UiRect chip = {};
    int line_x = 0;
    UiRect line = {};       // rail rect (empty when no body)
    bool is_empty = false;
    int item_x = 0;
    int item_width = 0;
    int empty_body_top = 0;
    int empty_body_height = 0;
    std::vector<RenderItem> items = {};
    bool has_scrollbar = false;
    UiRect scrollbar = {};
    UiRect thumb = {};
    uint8_t thumb_color = 0;
    std::string sticky_text = {};
    UiRect sticky_bounds = {};
};

std::vector<RenderGroup> ComputeRenderGroups(int canvas_width, int canvas_height,
                                             int origin_x, int origin_y,
                                             const TimelineListState& state,
                                             const TimelineListStyle& style)
{
    std::vector<RenderGroup> rendered;
    const int width = ResolveWidth(canvas_width, origin_x, style);
    const int max_height = ResolveHeight(canvas_height, origin_y, style);
    if (width <= 0 || max_height <= 0) {
        return rendered;
    }

    const int group_count = static_cast<int>(state.groups.size());
    const int group_gap = ClampPositive(style.group_gap);
    const bool list_active = HasActiveList(state);
    const VisibleGroupRangeResult visible_group_range =
        VisibleGroupRange(canvas_width, canvas_height, state, origin_x, origin_y, style);
    const int viewport_bottom = origin_y + max_height;

    int cursor_y = origin_y;
    for (int group_index = visible_group_range.first; group_index < visible_group_range.end;
         ++group_index) {
        if (cursor_y >= viewport_bottom) {
            break;
        }
        const TimelineGroupState& group = state.groups[static_cast<size_t>(group_index)];
        const bool group_focused =
            group_index == ClampIndex(state.focused_group_index, group_count) ||
            group_index == ClampIndex(state.active_group_index, group_count);
        const UiRect label_outer =
            MeasureLabelOuterBounds(origin_x, cursor_y, group.label_text, style);
        if (label_outer.IsEmpty() || label_outer.bottom() > viewport_bottom) {
            break;
        }

        RenderGroup render = {};
        render.group_index = group_index;
        render.focused = group_focused;
        render.active = list_active && state.active_group_index == group_index;
        render.chip = MeasureLabelChip(origin_x, cursor_y, group.label_text, style);
        cursor_y = label_outer.bottom();

        const TimelineGroupViewportLayout body_layout = BuildGroupViewportLayout(
            state, group, group_index, origin_x, cursor_y, width, viewport_bottom, max_height,
            label_outer.height, render.active, style);
        const int group_body_bottom =
            std::min(viewport_bottom, cursor_y + MaxGroupBodyHeight(max_height, label_outer.height, style));
        render.line_x = body_layout.line_x;
        render.item_x = body_layout.item_x;
        render.item_width = body_layout.item_width;

        if (group.items.empty()) {
            const int body_height = std::max(0, body_layout.available_body_height);
            if (body_height > 0) {
                render.is_empty = true;
                render.empty_body_top = cursor_y;
                render.empty_body_height = body_height;
                render.line = {body_layout.line_x, cursor_y,
                               std::max(1, ClampPositive(style.line_thickness)), body_height};
            }
            rendered.push_back(std::move(render));
            break;
        }

        const int first_visible_item = body_layout.first_visible_item;
        const int end_visible_item = body_layout.end_visible_item;
        const int sticky_footer_height = body_layout.sticky_footer_height;
        if (end_visible_item <= first_visible_item) {
            break;
        }

        int item_cursor_y = cursor_y;
        for (int item_index = first_visible_item; item_index < end_visible_item; ++item_index) {
            ListItemState item_state = group.items[static_cast<size_t>(item_index)];
            const bool selected = render.active && item_index == state.selected_item_index;
            item_state.selected = selected;
            const int item_height = MeasureItemHeight(item_state, body_layout.item_width, style);
            if (item_cursor_y + item_height > group_body_bottom - sticky_footer_height) {
                break;
            }
            RenderItem render_item = {};
            render_item.item_index = item_index;
            render_item.bounds = {body_layout.item_x, item_cursor_y, body_layout.item_width,
                                  item_height};
            render_item.selected = selected;
            render_item.last = item_index == static_cast<int>(group.items.size()) - 1;
            render.items.push_back(render_item);
            item_cursor_y += item_height;
            if (item_cursor_y >= group_body_bottom - sticky_footer_height) {
                break;
            }
        }

        const int rendered_body_height = std::max(0, item_cursor_y - cursor_y);
        const int line_height = rendered_body_height + sticky_footer_height;
        if (line_height > 0) {
            render.line = {body_layout.line_x, cursor_y,
                           std::max(1, ClampPositive(style.line_thickness)), line_height};
        }
        if (body_layout.has_scrollbar && line_height > 0) {
            render.has_scrollbar = true;
            render.scrollbar = GroupScrollbarBounds(origin_x + width, cursor_y, line_height, style);
            render.thumb = GroupScrollThumbBounds(render.scrollbar,
                                                  static_cast<int>(group.items.size()),
                                                  first_visible_item, end_visible_item, style);
            render.thumb_color = style.scrollbar_inactive_color;
            if (render.active) {
                render.thumb_color = style.scrollbar_focused_color;
            } else if (group_focused) {
                render.thumb_color = style.scrollbar_disabled_focused_color;
            }
        }
        if (!body_layout.sticky_footer_text.empty() && sticky_footer_height > 0) {
            render.sticky_text = body_layout.sticky_footer_text;
            render.sticky_bounds = {body_layout.item_x, cursor_y + rendered_body_height,
                                    body_layout.item_width, sticky_footer_height};
        }
        rendered.push_back(std::move(render));
        cursor_y = cursor_y + line_height + group_gap;
    }
    return rendered;
}

}  // namespace

TimelineLayout BuildTimelineLayout(int portrait_width, int portrait_height, int origin_x,
                                   int origin_y, const TimelineListState& state,
                                   const TimelineListStyle& style)
{
    TimelineLayout layout = {};
    const std::vector<RenderGroup> rendered =
        ComputeRenderGroups(portrait_width, portrait_height, origin_x, origin_y, state, style);
    layout.groups.reserve(rendered.size());
    for (const RenderGroup& render : rendered) {
        TimelineGroupHit hit = {};
        hit.group_index = render.group_index;
        hit.focused = render.focused;
        hit.active = render.active;
        hit.chip_bounds = render.chip;
        hit.items.reserve(render.items.size());
        for (const RenderItem& item : render.items) {
            hit.items.push_back({item.item_index, item.bounds});
        }
        layout.groups.push_back(std::move(hit));
    }
    return layout;
}

void DrawTimelineList(uint8_t* framebuffer, int raw_width, int raw_height, int portrait_width,
                      int portrait_height, int origin_x, int origin_y,
                      const TimelineListState& state, const TimelineListStyle& style)
{
    const std::vector<RenderGroup> rendered =
        ComputeRenderGroups(portrait_width, portrait_height, origin_x, origin_y, state, style);

    for (const RenderGroup& render : rendered) {
        // Focus ring behind the chip.
        if (render.focused && !render.chip.IsEmpty()) {
            const UiRect focus_bounds = ExpandRect(render.chip, FocusPadding(style));
            const UiRect focus_gap_bounds = ExpandRect(render.chip, ClampPositive(style.focus_gap));
            const int chip_radius = LabelCornerRadius(style);
            FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                    portrait_height, focus_bounds, chip_radius + FocusPadding(style),
                                    ResolveFocusRingColor(state, style));
            FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                    portrait_height, focus_gap_bounds,
                                    chip_radius + ClampPositive(style.focus_gap),
                                    style.focus_gap_color);
        }
        // Chip.
        if (!render.chip.IsEmpty()) {
            FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width,
                                    portrait_height, render.chip, LabelCornerRadius(style),
                                    style.label_background_color);
            const auto& group = state.groups[static_cast<size_t>(render.group_index)];
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               render.chip.x + ClampPositive(style.label_horizontal_padding),
                               render.chip.y + ClampPositive(style.label_vertical_padding),
                               group.label_text, style.label_role, style.label_text_color);
        }
        // Rail.
        if (!render.line.IsEmpty()) {
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             render.line, style.line_color);
        }
        // Empty-state icon + text.
        if (render.is_empty && render.empty_body_height > 0) {
            const int icon_size =
                state.empty_state_icon_asset != nullptr ? ClampPositive(style.empty_state_icon_size)
                                                        : 0;
            const int empty_state_gap = state.empty_state_icon_asset != nullptr
                                            ? ClampPositive(style.empty_state_gap)
                                            : 0;
            const int text_height = LineHeight(style.empty_state_role);
            const int stack_height = icon_size + empty_state_gap + text_height;
            const int stack_top = CenterY(render.empty_body_top, render.empty_body_height, stack_height);
            if (state.empty_state_icon_asset != nullptr && icon_size > 0) {
                const UiRect icon_bounds = {
                    CenterX(render.item_x, render.item_width, icon_size), stack_top, icon_size,
                    icon_size};
                DrawScaledPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                            portrait_height, icon_bounds,
                                            state.empty_state_icon_asset, style.empty_state_icon_color);
            }
            const std::string empty_text =
                FitLabelText(style.empty_state_role, state.empty_state_text, render.item_width);
            if (!empty_text.empty()) {
                const int text_width = MeasureText(style.empty_state_role, empty_text);
                DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                   portrait_height, CenterX(render.item_x, render.item_width, text_width),
                                   stack_top + icon_size + empty_state_gap, empty_text,
                                   style.empty_state_role, style.empty_state_icon_color);
            }
        }
        // Item rows.
        for (const RenderItem& item : render.items) {
            const auto& group = state.groups[static_cast<size_t>(render.group_index)];
            ListItemStyle item_style = style.item;
            item_style.width = item.bounds.width;
            if (item.last) {
                item_style.bottom_border_thickness = 0;
            }
            ListItemState item_state = group.items[static_cast<size_t>(item.item_index)];
            item_state.selected = item.selected;
            DrawListItem(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                         item.bounds.x, item.bounds.y, item_state, item_style);
        }
        // Scrollbar.
        if (render.has_scrollbar) {
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             render.scrollbar, style.scrollbar_track_color);
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             render.thumb, render.thumb_color);
            DrawPortraitBorder(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               render.scrollbar, ClampPositive(style.scrollbar_border_thickness),
                               style.scrollbar_border_color);
        }
        // Sticky footer count.
        if (!render.sticky_text.empty() && !render.sticky_bounds.IsEmpty()) {
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             render.sticky_bounds, style.sticky_footer_background_color);
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             {render.sticky_bounds.x, render.sticky_bounds.y, render.sticky_bounds.width, 2},
                             style.focus_gap_color);
            const int text_max_width = std::max(
                0, render.sticky_bounds.width - (2 * ClampPositive(style.sticky_footer_horizontal_padding)));
            const std::string fitted =
                FitLabelText(style.sticky_footer_role, render.sticky_text, text_max_width);
            if (!fitted.empty()) {
                const int text_x =
                    render.sticky_bounds.x + ClampPositive(style.sticky_footer_horizontal_padding);
                const int text_y =
                    render.sticky_bounds.y + ClampPositive(style.sticky_footer_vertical_padding);
                const int stroke = ClampPositive(style.sticky_footer_stroke_thickness);
                for (int dy = -stroke; dy <= stroke; ++dy) {
                    for (int dx = -stroke; dx <= stroke; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }
                        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                           portrait_height, text_x + dx, text_y + dy, fitted,
                                           style.sticky_footer_role, style.focus_gap_color);
                    }
                }
                DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                   portrait_height, text_x, text_y, fitted, style.sticky_footer_role,
                                   style.sticky_footer_text_color);
            }
        }
    }
}

}  // namespace epaper_ui
