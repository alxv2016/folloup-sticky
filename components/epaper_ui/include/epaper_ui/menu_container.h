#ifndef EPAPER_UI_MENU_CONTAINER_H_
#define EPAPER_UI_MENU_CONTAINER_H_

#include <cstdint>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

inline constexpr int kMenuContainerNoSelection = -1;

enum class MenuContainerDirection : uint8_t {
    kVertical,
    kHorizontal,
};

enum class MenuContainerSizing : uint8_t {
    kFixedItemExtent,
    kFillEqual,
};

// Layout-only helper: computes the container bounds and each item's bounds. Item contents
// (label, badge, selection highlight) are drawn by the owning page using ItemBounds.
struct MenuContainerState {
    int selected_index = kMenuContainerNoSelection;
    int item_count = 0;

    bool operator==(const MenuContainerState& other) const = default;
};

struct MenuContainerStyle {
    MenuContainerDirection direction = MenuContainerDirection::kVertical;
    MenuContainerSizing sizing = MenuContainerSizing::kFixedItemExtent;
    int width = 0;
    int height = 0;
    int item_width = 0;
    int item_height = 0;
    int item_gap = 0;
};

UiRect MenuContainerBounds(int origin_x,
                           int origin_y,
                           const MenuContainerState& state,
                           const MenuContainerStyle& style);
UiRect MenuContainerItemBounds(int origin_x,
                               int origin_y,
                               const MenuContainerState& state,
                               const MenuContainerStyle& style,
                               int index);

}  // namespace epaper_ui

#endif  // EPAPER_UI_MENU_CONTAINER_H_
