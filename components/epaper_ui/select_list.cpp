#include "epaper_ui/select_list.h"

#include <algorithm>
#include <utility>

#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kPanelCornerRadius = 4;

int FocusPadding(const SelectListStyle& style)
{
    return ClampPositive(style.focus_ring_thickness) + ClampPositive(style.focus_gap);
}

int ClampSelectedItemIndex(const SelectListState& state)
{
    return state.selected_item_index >= 0 &&
                   state.selected_item_index < static_cast<int>(state.items.size())
               ? state.selected_item_index
               : kSelectListNoSelection;
}

bool HasFocusRing(const SelectListState& state)
{
    return state.focused || state.active;
}

int VisibleRowCapacity(const UiRect& viewport, const SelectItemStyle& item_style)
{
    const int item_height = ClampPositive(item_style.height);
    if (item_height <= 0) {
        return 0;
    }
    return std::max(1, viewport.height / item_height);
}

std::pair<int, int> VisibleRange(const SelectListState& state,
                                 const UiRect& viewport,
                                 const SelectItemStyle& item_style)
{
    const int item_count = static_cast<int>(state.items.size());
    if (item_count <= 0) {
        return {0, 0};
    }

    const int capacity = VisibleRowCapacity(viewport, item_style);
    if (capacity <= 0) {
        return {0, 0};
    }
    if (item_count <= capacity) {
        return {0, item_count};
    }

    const int selected_index = ClampSelectedItemIndex(state);
    if (selected_index == kSelectListNoSelection) {
        return {0, capacity};
    }

    int first_visible = selected_index - capacity + 1;
    first_visible = std::clamp(first_visible, 0, item_count - capacity);
    return {first_visible, first_visible + capacity};
}

}  // namespace

UiRect SelectListPanelBounds(int origin_x, int origin_y, const SelectListStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.panel_height)};
}

UiRect SelectListViewportBounds(int origin_x, int origin_y, const SelectListStyle& style)
{
    const UiRect panel = SelectListPanelBounds(origin_x, origin_y, style);
    const int inset = ClampPositive(style.panel_border_thickness);
    return {panel.x + inset,
            panel.y + inset,
            std::max(0, panel.width - (2 * inset)),
            std::max(0, panel.height - (2 * inset))};
}

UiRect SelectListItemBounds(int origin_x,
                            int origin_y,
                            const SelectListState& state,
                            const SelectListStyle& style,
                            int index)
{
    const UiRect viewport = SelectListViewportBounds(origin_x, origin_y, style);
    if (viewport.IsEmpty() || index < 0 || index >= static_cast<int>(state.items.size())) {
        return {};
    }

    const auto [first_visible, end_visible] = VisibleRange(state, viewport, style.item);
    if (index < first_visible || index >= end_visible) {
        return {};
    }

    const int visible_row = index - first_visible;
    return {viewport.x, viewport.y + (visible_row * ClampPositive(style.item.height)), viewport.width,
            ClampPositive(style.item.height)};
}

void DrawSelectList(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const SelectListState& state,
                    const SelectListStyle& style)
{
    const UiRect panel = SelectListPanelBounds(origin_x, origin_y, style);
    if (panel.IsEmpty() || style.item.height <= 0) {
        return;
    }

    if (HasFocusRing(state)) {
        const int ring_inset = FocusPadding(style);
        const UiRect focus_bounds = {panel.x - ring_inset,
                                     panel.y - ring_inset,
                                     panel.width + (2 * ring_inset),
                                     panel.height + (2 * ring_inset)};
        const UiRect focus_gap_bounds = {panel.x - ClampPositive(style.focus_gap),
                                         panel.y - ClampPositive(style.focus_gap),
                                         panel.width + (2 * ClampPositive(style.focus_gap)),
                                         panel.height + (2 * ClampPositive(style.focus_gap))};
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                focus_bounds,
                                kPanelCornerRadius + ring_inset,
                                style.focus_ring_color);
        FillRoundedPortraitRect(framebuffer,
                                raw_width,
                                raw_height,
                                portrait_width,
                                portrait_height,
                                focus_gap_bounds,
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

    const UiRect viewport = SelectListViewportBounds(origin_x, origin_y, style);
    const auto [first_visible, end_visible] = VisibleRange(state, viewport, style.item);
    int row_y = viewport.y;
    for (int index = first_visible; index < end_visible; ++index) {
        SelectItemState item_state = state.items[static_cast<size_t>(index)];
        item_state.selected = state.active && index == ClampSelectedItemIndex(state);

        SelectItemStyle item_style = style.item;
        item_style.width = viewport.width;
        DrawSelectItem(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       viewport.x,
                       row_y,
                       item_state,
                       item_style);
        row_y += ClampPositive(item_style.height);
    }
}

}  // namespace epaper_ui
