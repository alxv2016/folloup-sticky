#include "epaper_ui/network_list.h"

#include <algorithm>
#include <cstdio>

#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kPanelCornerRadius = 4;

int FocusPadding(const NetworkListStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

UiRect Inset(const UiRect& rect, int inset)
{
    return {rect.x + inset,
            rect.y + inset,
            std::max(0, rect.width - (2 * inset)),
            std::max(0, rect.height - (2 * inset))};
}

void DrawScaledMonoAsset(uint8_t* framebuffer,
                         int raw_width,
                         int raw_height,
                         int portrait_width,
                         int portrait_height,
                         const EmbeddedImageAsset* asset,
                         const UiRect& bounds,
                         uint8_t tone)
{
    if (framebuffer == nullptr || asset == nullptr || asset->format != ImageFormat::kMono1 ||
        bounds.IsEmpty() || asset->width == 0 || asset->height == 0) {
        return;
    }

    for (int y = 0; y < bounds.height; ++y) {
        const int source_y = std::min<int>(
            static_cast<int>(asset->height) - 1,
            (static_cast<int64_t>(y) * static_cast<int>(asset->height)) / bounds.height);
        for (int x = 0; x < bounds.width; ++x) {
            const int source_x = std::min<int>(
                static_cast<int>(asset->width) - 1,
                (static_cast<int64_t>(x) * static_cast<int>(asset->width)) / bounds.width);
            const size_t byte_index = static_cast<size_t>(source_y) * asset->stride_bytes +
                                      static_cast<size_t>(source_x / 8);
            const uint8_t mask = static_cast<uint8_t>(0x80U >> (source_x & 0x07));
            if ((asset->data[byte_index] & mask) == 0) {
                continue;
            }
            const int px = bounds.x + x;
            const int py = bounds.y + y;
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

int ClampSelectedIndex(const NetworkListState& state)
{
    return state.selected_network_index >= 0 &&
                   state.selected_network_index < static_cast<int>(state.networks.size())
               ? state.selected_network_index
               : kNetworkListNoSelection;
}

int ClampFocusedIndex(const NetworkListState& state)
{
    return state.focused_network_index >= 0 &&
                   state.focused_network_index < static_cast<int>(state.networks.size())
               ? state.focused_network_index
               : kNetworkListNoSelection;
}

int VisibleRowCapacity(const UiRect& viewport, const NetworkListStyle& style)
{
    const int row_height = ClampPositive(style.item.height);
    if (row_height <= 0 || viewport.IsEmpty()) {
        return 0;
    }
    return std::max(1, viewport.height / row_height);
}

// Top Y of the row at 0-based position `row_in_view`, distributing the viewport height
// evenly across `capacity` rows so the visible rows fill the panel exactly (no leftover
// gap at the bottom of the list). Each row's height is the difference between consecutive
// edges; rows are >= the nominal item height since capacity = floor(viewport / item).
int RowEdgeY(const UiRect& viewport, int capacity, int row_in_view)
{
    if (capacity <= 0) {
        return viewport.y;
    }
    return viewport.y + (row_in_view * viewport.height) / capacity;
}

int AnchorIndex(const NetworkListState& state)
{
    const int focused = ClampFocusedIndex(state);
    if (state.active && focused != kNetworkListNoSelection) {
        return focused;
    }
    return ClampSelectedIndex(state);
}

void VisibleRange(const NetworkListState& state,
                  const NetworkListStyle& style,
                  const UiRect& viewport,
                  int* first_out,
                  int* end_out)
{
    if (first_out != nullptr) {
        *first_out = 0;
    }
    if (end_out != nullptr) {
        *end_out = 0;
    }

    const int count = static_cast<int>(state.networks.size());
    const int capacity = VisibleRowCapacity(viewport, style);
    if (count <= 0 || capacity <= 0) {
        return;
    }
    if (count <= capacity) {
        if (end_out != nullptr) {
            *end_out = count;
        }
        return;
    }

    const int anchor = AnchorIndex(state);
    if (anchor == kNetworkListNoSelection) {
        if (end_out != nullptr) {
            *end_out = capacity;
        }
        return;
    }

    int first = anchor - capacity + 1;
    first = std::clamp(first, 0, count - capacity);
    if (first_out != nullptr) {
        *first_out = first;
    }
    if (end_out != nullptr) {
        *end_out = first + capacity;
    }
}

std::string StatusText(const NetworkListState& state)
{
    switch (state.status) {
        case NetworkListStatus::kNetworksFound: {
            char buffer[32] = {};
            const int count = static_cast<int>(state.networks.size());
            std::snprintf(buffer,
                          sizeof(buffer),
                          "%d %s found",
                          count,
                          count == 1 ? "network" : "networks");
            return std::string(buffer);
        }
        case NetworkListStatus::kNoNetworks:
            return "No networks found";
        case NetworkListStatus::kIdle:
        default:
            return {};
    }
}

const char* EmptyStateText(const NetworkListState& state)
{
    switch (state.status) {
        case NetworkListStatus::kNoNetworks:
            return "No WiFi networks found";
        case NetworkListStatus::kIdle:
        case NetworkListStatus::kNetworksFound:
        default:
            return "Scanning for networks...";
    }
}

uint8_t FocusRingColor(const NetworkListState& state, const NetworkListStyle& style)
{
    return state.active ? style.active_focus_ring_color : style.inactive_focus_ring_color;
}

bool ShowFocusRing(const NetworkListState& state)
{
    return state.focus_ring_visible && (state.focused || state.active);
}

bool IsScrollbarFocused(const NetworkListState& state)
{
    return state.active;
}

UiRect ScrollbarBounds(int origin_x, int origin_y, const NetworkListStyle& style)
{
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    const UiRect inner = Inset(panel, ClampPositive(style.panel_border_thickness));
    const int width = ClampPositive(style.scrollbar_width);
    return {inner.right() - width, inner.y, width, inner.height};
}

UiRect ScrollThumbBounds(int origin_x,
                         int origin_y,
                         const NetworkListState& state,
                         const NetworkListStyle& style)
{
    const UiRect scrollbar = ScrollbarBounds(origin_x, origin_y, style);
    const UiRect inner = Inset(scrollbar, ClampPositive(style.scrollbar_border_thickness));
    if (inner.IsEmpty()) {
        return {};
    }

    const UiRect viewport = NetworkListViewportBounds(origin_x, origin_y, style);
    const int total_rows = static_cast<int>(state.networks.size());
    const int visible_rows = VisibleRowCapacity(viewport, style);
    if (total_rows <= 0 || visible_rows <= 0 || total_rows <= visible_rows) {
        return inner;
    }

    const int thumb_height = std::clamp((inner.height * visible_rows + (total_rows / 2)) / total_rows,
                                        std::min(inner.height, ClampPositive(style.min_thumb_height)),
                                        inner.height);
    int first_visible = 0;
    int end_visible = 0;
    VisibleRange(state, style, viewport, &first_visible, &end_visible);
    const int max_offset = std::max(1, total_rows - visible_rows);
    const int travel = std::max(0, inner.height - thumb_height);
    const int thumb_y = inner.y + ((travel * first_visible) + (max_offset / 2)) / max_offset;
    return {inner.x, thumb_y, inner.width, thumb_height};
}

}  // namespace

UiRect NetworkListPanelBounds(int origin_x, int origin_y, const NetworkListStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.panel_height)};
}

UiRect NetworkListViewportBounds(int origin_x, int origin_y, const NetworkListStyle& style)
{
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    const UiRect inner = Inset(panel, ClampPositive(style.panel_border_thickness));
    const UiRect scrollbar = ScrollbarBounds(origin_x, origin_y, style);
    const int scrollbar_gap = scrollbar.width > 0 ? ClampPositive(style.scrollbar_gap) : 0;
    return {inner.x,
            inner.y,
            std::max(0, scrollbar.x - scrollbar_gap - inner.x),
            inner.height};
}

UiRect NetworkListStatusBounds(int origin_x,
                               int origin_y,
                               const NetworkListState& state,
                               const NetworkListStyle& style)
{
    const std::string text = StatusText(state);
    if (text.empty()) {
        return {};
    }
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    return {panel.x,
            panel.bottom() + ClampPositive(style.section_gap),
            MeasureText(style.status_role, text),
            LineHeight(style.status_role)};
}

UiRect NetworkListBounds(int origin_x, int origin_y, const NetworkListStyle& style)
{
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    const UiRect status = NetworkListStatusBounds(origin_x, origin_y, {}, style);
    return {panel.x,
            panel.y,
            std::max(panel.width, status.width),
            panel.height + (status.IsEmpty() ? 0 : ClampPositive(style.section_gap) + status.height)};
}

UiRect NetworkListVisualBounds(int origin_x,
                               int origin_y,
                               const NetworkListState& state,
                               const NetworkListStyle& style)
{
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    UiRect visual_bounds = panel;
    if (ShowFocusRing(state)) {
        const int ring_padding = FocusPadding(style);
        visual_bounds = {panel.x - ring_padding,
                         panel.y - ring_padding,
                         panel.width + (2 * ring_padding),
                         panel.height + (2 * ring_padding)};
    }
    return UnionRect(visual_bounds, NetworkListStatusBounds(origin_x, origin_y, state, style));
}

int NetworkListFirstVisibleRow(int origin_x,
                               int origin_y,
                               const NetworkListState& state,
                               const NetworkListStyle& style)
{
    const UiRect viewport = NetworkListViewportBounds(origin_x, origin_y, style);
    int first = 0;
    VisibleRange(state, style, viewport, &first, nullptr);
    return first;
}

UiRect NetworkListRowBounds(int origin_x,
                            int origin_y,
                            const NetworkListState& state,
                            const NetworkListStyle& style,
                            int network_index)
{
    const UiRect viewport = NetworkListViewportBounds(origin_x, origin_y, style);
    if (viewport.IsEmpty() || network_index < 0 ||
        network_index >= static_cast<int>(state.networks.size())) {
        return {};
    }

    int first = 0;
    int end = 0;
    VisibleRange(state, style, viewport, &first, &end);
    if (network_index < first || network_index >= end) {
        return {};
    }
    const int capacity = VisibleRowCapacity(viewport, style);
    const int row_top = RowEdgeY(viewport, capacity, network_index - first);
    return {viewport.x,
            row_top,
            viewport.width,
            RowEdgeY(viewport, capacity, network_index - first + 1) - row_top};
}

bool HitTestNetworkListRow(int origin_x,
                           int origin_y,
                           const NetworkListState& state,
                           const NetworkListStyle& style,
                           int x,
                           int y,
                           int* network_index)
{
    if (network_index != nullptr) {
        *network_index = kNetworkListNoSelection;
    }
    for (int index = 0; index < static_cast<int>(state.networks.size()); ++index) {
        const UiRect bounds = NetworkListRowBounds(origin_x, origin_y, state, style, index);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (network_index != nullptr) {
                *network_index = index;
            }
            return true;
        }
    }
    return false;
}

void DrawNetworkList(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const NetworkListState& state,
                     const NetworkListStyle& style)
{
    const UiRect panel = NetworkListPanelBounds(origin_x, origin_y, style);
    if (panel.IsEmpty()) {
        return;
    }

    if (ShowFocusRing(state)) {
        const int ring_padding = FocusPadding(style);
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                {panel.x - ring_padding,
                                 panel.y - ring_padding,
                                 panel.width + (2 * ring_padding),
                                 panel.height + (2 * ring_padding)},
                                kPanelCornerRadius + ring_padding,
                                FocusRingColor(state, style));
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                {panel.x - ClampPositive(style.focus_gap),
                                 panel.y - ClampPositive(style.focus_gap),
                                 panel.width + (2 * ClampPositive(style.focus_gap)),
                                 panel.height + (2 * ClampPositive(style.focus_gap))},
                                kPanelCornerRadius + ClampPositive(style.focus_gap),
                                style.focus_gap_color);
    }

    FillRoundedPortraitRect(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            panel,
                            kPanelCornerRadius,
                            style.panel_background_color);
    DrawRoundedPortraitBorder(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              panel,
                              kPanelCornerRadius,
                              style.panel_border_thickness,
                              style.panel_border_color);

    const UiRect viewport = NetworkListViewportBounds(origin_x, origin_y, style);
    if (state.networks.empty()) {
        const EmbeddedImageAsset* icon = project_assets::GetIcon(EmbeddedIconId::kWifiConfig);
        if (icon != nullptr) {
            const int icon_y = viewport.y + std::max(
                                               0,
                                               (viewport.height - style.empty_state_icon_size -
                                                LineHeight(style.empty_state_role) -
                                                ClampPositive(style.empty_state_gap)) /
                                                   2);
            DrawScaledMonoAsset(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                icon,
                                {viewport.x + CenterOffset(viewport.width, style.empty_state_icon_size),
                                 icon_y,
                                 style.empty_state_icon_size,
                                 style.empty_state_icon_size},
                                style.empty_state_icon_color);
            const std::string_view text = EmptyStateText(state);
            DrawTypographyText(framebuffer,
                               raw_width,
                               raw_height,
                               portrait_width,
                               portrait_height,
                               viewport.x + CenterOffset(viewport.width,
                                                          MeasureText(style.empty_state_role, text)),
                               icon_y + style.empty_state_icon_size + ClampPositive(style.empty_state_gap),
                               text,
                               style.empty_state_role,
                               style.empty_state_text_color);
        }
    } else {
        int first = 0;
        int end = 0;
        VisibleRange(state, style, viewport, &first, &end);
        const int capacity = VisibleRowCapacity(viewport, style);
        for (int index = first; index < end; ++index) {
            NetworkItemState row = state.networks[static_cast<size_t>(index)];
            NetworkItemStyle row_style = style.item;
            const bool row_is_selected = index == ClampSelectedIndex(state);
            const bool row_is_focused = state.active && index == ClampFocusedIndex(state);
            if (row_is_focused) {
                row_style.selected_background_color = style.item.selected_background_color;
                row_style.selected_text_color = style.item.selected_text_color;
                row_style.selected_icon_color = style.item.selected_icon_color;
                row_style.selected_content_outlined = false;
            } else if (row_is_selected) {
                row_style.selected_background_color = design::vibe_card::kFooterButtonBackgroundColor;
                row_style.selected_text_color = design::color::kBlack;
                row_style.selected_icon_color = design::color::kBlack;
                row_style.selected_content_outline_color = design::color::kWhite;
                row_style.selected_content_stroke_thickness = design::button::kStrokeThickness;
                row_style.selected_content_outlined = true;
            }
            row.selected = row_is_focused || row_is_selected;
            const int row_top = RowEdgeY(viewport, capacity, index - first);
            row_style.width = viewport.width;
            row_style.height = RowEdgeY(viewport, capacity, index - first + 1) - row_top;
            DrawNetworkItem(framebuffer,
                            raw_width,
                            raw_height,
                            portrait_width,
                            portrait_height,
                            viewport.x,
                            row_top,
                            row,
                            row_style);
        }
    }

    const UiRect scrollbar = ScrollbarBounds(origin_x, origin_y, style);
    const UiRect thumb = ScrollThumbBounds(origin_x, origin_y, state, style);
    if (!scrollbar.IsEmpty()) {
        const bool has_overflow =
            static_cast<int>(state.networks.size()) > VisibleRowCapacity(viewport, style);
        const uint8_t thumb_color =
            has_overflow ? (IsScrollbarFocused(state) ? style.scrollbar_focused_color
                                                      : style.scrollbar_inactive_color)
                         : (IsScrollbarFocused(state) ? style.scrollbar_disabled_focused_color
                                                      : style.scrollbar_inactive_color);
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         Inset(scrollbar, ClampPositive(style.scrollbar_border_thickness)),
                         style.scrollbar_track_color);
        DrawPortraitBorder(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           scrollbar,
                           style.scrollbar_border_thickness,
                           style.scrollbar_border_color);
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         thumb,
                         thumb_color);
    }

    const std::string status_text = StatusText(state);
    if (!status_text.empty()) {
        const UiRect status = NetworkListStatusBounds(origin_x, origin_y, state, style);
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           status.x,
                           status.y,
                           status_text,
                           style.status_role,
                           style.status_text_color);
    }
}

}  // namespace epaper_ui
