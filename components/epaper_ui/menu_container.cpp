#include "epaper_ui/menu_container.h"

#include <algorithm>

#include "render_utils.h"

namespace epaper_ui {

UiRect MenuContainerBounds(int origin_x,
                           int origin_y,
                           const MenuContainerState& state,
                           const MenuContainerStyle& style)
{
    const int item_count = ClampPositive(state.item_count);
    const int gap = ClampPositive(style.item_gap);

    int width = ClampPositive(style.width);
    int height = ClampPositive(style.height);

    if (style.direction == MenuContainerDirection::kVertical) {
        if (height == 0 && style.sizing == MenuContainerSizing::kFixedItemExtent) {
            height = (item_count * ClampPositive(style.item_height)) +
                     (std::max(0, item_count - 1) * gap);
        }
    } else {
        if (width == 0 && style.sizing == MenuContainerSizing::kFixedItemExtent) {
            width = (item_count * ClampPositive(style.item_width)) +
                    (std::max(0, item_count - 1) * gap);
        }
        if (height == 0) {
            height = ClampPositive(style.item_height);
        }
    }
    return {origin_x, origin_y, width, height};
}

UiRect MenuContainerItemBounds(int origin_x,
                               int origin_y,
                               const MenuContainerState& state,
                               const MenuContainerStyle& style,
                               int index)
{
    const int item_count = ClampPositive(state.item_count);
    if (index < 0 || index >= item_count) {
        return {origin_x, origin_y, 0, 0};
    }

    const UiRect bounds = MenuContainerBounds(origin_x, origin_y, state, style);
    const int gap = ClampPositive(style.item_gap);

    if (style.direction == MenuContainerDirection::kVertical) {
        const int item_height =
            style.sizing == MenuContainerSizing::kFillEqual && item_count > 0
                ? std::max(0, (bounds.height - (gap * std::max(0, item_count - 1))) / item_count)
                : ClampPositive(style.item_height);
        return {bounds.x, bounds.y + (index * (item_height + gap)), bounds.width, item_height};
    }

    if (style.sizing == MenuContainerSizing::kFillEqual && item_count > 0) {
        const int total_gap = gap * std::max(0, item_count - 1);
        const int available_width = std::max(0, bounds.width - total_gap);
        const int base_width = available_width / item_count;
        const int remainder = available_width % item_count;
        int cursor_x = bounds.x;
        for (int item_index = 0; item_index < item_count; ++item_index) {
            const int item_width = base_width + (item_index < remainder ? 1 : 0);
            if (item_index == index) {
                return {cursor_x, bounds.y, item_width, bounds.height};
            }
            cursor_x += item_width + gap;
        }
    }

    const int item_width = ClampPositive(style.item_width);
    return {bounds.x + (index * (item_width + gap)), bounds.y, item_width, bounds.height};
}

}  // namespace epaper_ui
