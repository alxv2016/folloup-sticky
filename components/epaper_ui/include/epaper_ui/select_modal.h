#ifndef EPAPER_UI_SELECT_MODAL_H_
#define EPAPER_UI_SELECT_MODAL_H_

#include <string>
#include <vector>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

struct SelectModalItemState {
    std::string label_text = {};
};

struct SelectModalState {
    bool visible = false;
    std::string title_text = {};
    int selected_index = 0;
    std::vector<SelectModalItemState> items = {};
};

UiRect SelectModalPanelBounds(int portrait_width,
                              int portrait_height,
                              const SelectModalState& state);
UiRect SelectModalItemBounds(int portrait_width,
                             int portrait_height,
                             const SelectModalState& state,
                             int index);
int HitTestSelectModalItem(int portrait_width,
                           int portrait_height,
                           const SelectModalState& state,
                           int x,
                           int y,
                           bool* hit);
void DrawSelectModal(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     const SelectModalState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SELECT_MODAL_H_
