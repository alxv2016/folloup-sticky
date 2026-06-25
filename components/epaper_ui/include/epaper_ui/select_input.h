#ifndef EPAPER_UI_SELECT_INPUT_H_
#define EPAPER_UI_SELECT_INPUT_H_

#include <string>

#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/text_input.h"

namespace epaper_ui {

// A read-only labeled field showing the currently selected value plus a trailing dropdown
// chevron. Activating it opens a selection modal (owned by the caller). Built on TextInput.
struct SelectInputState {
    std::string label_text = {};
    std::string placeholder_text = {};
    std::string value_text = {};
    bool focused = false;

    bool operator==(const SelectInputState& other) const = default;
};

UiRect SelectInputBounds(int origin_x,
                         int origin_y,
                         const SelectInputState& state,
                         const TextInputStyle& style);
UiRect SelectInputVisualBounds(int origin_x,
                               int origin_y,
                               const SelectInputState& state,
                               const TextInputStyle& style);
void DrawSelectInput(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const SelectInputState& state,
                     const TextInputStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_SELECT_INPUT_H_
