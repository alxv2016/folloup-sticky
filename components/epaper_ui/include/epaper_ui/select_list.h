#ifndef EPAPER_UI_SELECT_LIST_H_
#define EPAPER_UI_SELECT_LIST_H_

#include <cstdint>
#include <vector>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/select_item.h"

namespace epaper_ui {

inline constexpr int kSelectListNoSelection = -1;

struct SelectListState {
    std::vector<SelectItemState> items = {};
    bool focused = false;
    bool active = false;
    int selected_item_index = kSelectListNoSelection;
};

struct SelectListStyle {
    uint8_t panel_background_color = design::color::kGrayLight;
    uint8_t panel_border_color = design::color::kBlack;
    uint8_t focus_ring_color = design::color::kBlack;
    uint8_t focus_gap_color = design::color::kWhite;
    int width = 0;
    int panel_height = 0;
    int panel_border_thickness = design::network_list::kPanelBorderThickness;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
    SelectItemStyle item = {};
};

UiRect SelectListPanelBounds(int origin_x, int origin_y, const SelectListStyle& style);
UiRect SelectListViewportBounds(int origin_x, int origin_y, const SelectListStyle& style);
UiRect SelectListItemBounds(int origin_x,
                            int origin_y,
                            const SelectListState& state,
                            const SelectListStyle& style,
                            int index);
void DrawSelectList(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const SelectListState& state,
                    const SelectListStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SELECT_LIST_H_
