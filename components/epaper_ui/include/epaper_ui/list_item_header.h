#ifndef EPAPER_UI_LIST_ITEM_HEADER_H_
#define EPAPER_UI_LIST_ITEM_HEADER_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/overlay_geometry.h"
#include "epaper_ui/tag.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

// One metadata row: a leading icon, a "time · duration" pair separated by a dot, and a
// trailing right-aligned tag pill. Middle text is ellipsized to fit the space that is left.
struct ListItemHeaderState {
    const EmbeddedImageAsset* icon_asset = nullptr;
    // Optional trailing status icon (e.g. the follow-up "pin"), drawn just left of the tag pill.
    const EmbeddedImageAsset* tag_icon_asset = nullptr;
    std::string time_text = {};
    std::string minute_seconds_text = {};
    std::string tag_text = {};
    bool selected = false;

    bool operator==(const ListItemHeaderState& other) const = default;
};

struct ListItemHeaderStyle {
    design::TypographyRole role = design::TypographyRole::kLabelSmall;
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t icon_color = design::color::kBlack;
    uint8_t selected_icon_color = design::color::kWhite;
    uint8_t divider_color = design::color::kBlack;
    uint8_t selected_divider_color = design::color::kWhite;
    int width = 0;
    int height = design::list_item_header::kHeight;
    int icon_slot_size = design::list_item_header::kIconSlotSize;
    int content_gap = design::list_item_header::kContentGap;
    int divider_dot_diameter = design::list_item_header::kDividerDotDiameter;
    int tag_icon_slot_size = design::list_item_header::kTagIconSlotSize;
    int tag_icon_gap = design::list_item_header::kTagIconGap;
    // When selected on a filled background, stroke icons/text/dot with the knockout color so they
    // stay legible (used by the timeline's selected item rows).
    bool selected_content_outlined = false;
    int selected_content_stroke_thickness = design::button::kStrokeThickness;
    uint8_t selected_content_outline_color = design::color::kWhite;
    TagStyle tag = {};
};

UiRect ListItemHeaderBounds(int origin_x,
                            int origin_y,
                            const ListItemHeaderState& state,
                            const ListItemHeaderStyle& style);
void DrawListItemHeader(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int origin_x,
                        int origin_y,
                        const ListItemHeaderState& state,
                        const ListItemHeaderStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_LIST_ITEM_HEADER_H_
