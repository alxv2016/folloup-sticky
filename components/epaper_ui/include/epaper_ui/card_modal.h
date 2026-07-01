#ifndef EPAPER_UI_CARD_MODAL_H_
#define EPAPER_UI_CARD_MODAL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A generic centered card modal: a title, wrapped body text, and 0-2 action
// buttons at the bottom. Content is fully data-driven so the component is
// reusable across purposes (shutdown confirm; storage no-SD / confirm-format /
// formatting / success / error; etc.). What an action *does* is decided by the
// caller, not this component.
struct CardModalState {
    bool visible = false;
    std::string title_text = {};
    std::string body_text = {};
    // 0, 1, or 2 action button labels, laid out left-to-right. Empty = no
    // buttons (e.g. a progress card). More than two are ignored.
    std::vector<std::string> action_labels = {};
    int selected_action_index = 0;
};

// Number of action buttons actually laid out (action_labels size clamped to 2).
int CardModalActionCount(const CardModalState& state);

UiRect CardModalPanelBounds(int portrait_width,
                            int portrait_height,
                            const CardModalState& state);
UiRect CardModalActionBounds(int portrait_width,
                             int portrait_height,
                             const CardModalState& state,
                             int action_index);
// Returns the hit action index, or -1 if none. Sets *hit accordingly.
int HitTestCardModalAction(int portrait_width,
                           int portrait_height,
                           const CardModalState& state,
                           int x,
                           int y,
                           bool* hit);
void DrawCardModal(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const CardModalState& state);

}  // namespace epaper_ui

#endif  // EPAPER_UI_CARD_MODAL_H_
