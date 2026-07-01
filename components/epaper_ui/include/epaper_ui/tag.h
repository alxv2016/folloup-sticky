#ifndef EPAPER_UI_TAG_H_
#define EPAPER_UI_TAG_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

// A small rounded pill carrying a short label (e.g. "Idea"). Text is knockout-outlined
// when it sits on a light background so it stays legible.
struct TagState {
    std::string label_text = {};
    bool selected = false;

    bool operator==(const TagState& other) const = default;
};

struct TagStyle {
    design::TypographyRole role = design::TypographyRole::kLabelSmall;
    uint8_t background_color = design::status_bar::kBackgroundColor;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    int horizontal_padding = design::tag::kHorizontalPadding;
    int vertical_padding = design::tag::kVerticalPadding;
    int border_thickness = design::tag::kBorderThickness;
    int stroke_thickness = design::button::kStrokeThickness;
};

UiRect TagBounds(int origin_x, int origin_y, const TagState& state, const TagStyle& style);
bool TagIsValid(const TagState& state);
void DrawTag(uint8_t* framebuffer,
             int raw_width,
             int raw_height,
             int portrait_width,
             int portrait_height,
             int origin_x,
             int origin_y,
             const TagState& state,
             const TagStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TAG_H_
