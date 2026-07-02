#ifndef EPAPER_UI_CHECKBOX_H_
#define EPAPER_UI_CHECKBOX_H_

#include <cstdint>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A checkbox accessory: a checked/unchecked glyph, optionally on a rounded background, with an
// optional knockout outline so it reads on a filled/selected row.
struct CheckboxState {
    bool checked = false;
    bool selected = false;

    bool operator==(const CheckboxState& other) const = default;
};

struct CheckboxStyle {
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kWhite;
    uint8_t icon_color = design::color::kBlack;
    uint8_t selected_icon_color = design::color::kWhite;
    uint8_t selected_content_outline_color = design::color::kWhite;
    int size = design::button_icon::kSize;
    int selected_content_stroke_thickness = design::button::kStrokeThickness;
    int background_corner_radius = design::spacing::k4;
    bool background_visible = false;
    bool selected_background_visible = false;
    bool selected_content_outlined = false;
};

UiRect CheckboxBounds(int origin_x, int origin_y, const CheckboxStyle& style);
void DrawCheckbox(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const CheckboxState& state,
                  const CheckboxStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_CHECKBOX_H_
