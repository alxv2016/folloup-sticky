#ifndef EPAPER_UI_LIST_ITEM_H_
#define EPAPER_UI_LIST_ITEM_H_

#include <cstdint>
#include <string>

#include "design_tokens.h"
#include "epaper_ui/button_icon.h"
#include "epaper_ui/checkbox.h"
#include "epaper_ui/list_item_header.h"
#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

enum class ListItemAccessoryKind : uint8_t {
    kNone = 0,
    kCheckbox,
    kIcon,
};

struct ListItemAccessoryState {
    ListItemAccessoryKind kind = ListItemAccessoryKind::kNone;
    bool checked = false;
    const EmbeddedImageAsset* icon_asset = nullptr;

    bool operator==(const ListItemAccessoryState& other) const = default;
};

// Optional swipe/secondary action tray overlaid on the right edge (Close + Confirm icon buttons).
struct ListItemActionsState {
    bool visible = false;
    int selected_action_index = -1;

    bool operator==(const ListItemActionsState& other) const = default;
};

// One archive row: a metadata header over a body line, with an optional leading accessory
// (checkbox/icon) and an optional trailing action tray.
struct ListItemState {
    ListItemHeaderState header = {};
    std::string body_text = {};
    bool selected = false;
    ListItemAccessoryState accessory = {};
    ListItemActionsState actions = {};

    bool operator==(const ListItemState& other) const = default;
};

struct ListItemStyle {
    uint8_t background_color = design::color::kWhite;
    uint8_t selected_background_color = design::color::kBlack;
    uint8_t text_color = design::color::kBlack;
    uint8_t selected_text_color = design::color::kWhite;
    uint8_t selected_content_outline_color = design::color::kWhite;
    uint8_t border_color = design::color::kBlack;
    int width = 0;
    int padding_left = design::list_item::kPaddingLeft;
    int padding_right = design::list_item::kPaddingRight;
    int padding_top = design::list_item::kPaddingTop;
    int padding_bottom = design::list_item::kPaddingBottom;
    int row_gap = design::list_item::kRowGap;
    int accessory_gap = design::list_item::kAccessoryGap;
    int bottom_border_thickness = design::list_item::kBottomBorderThickness;
    int actions_gap = design::list_item::kActionsGap;
    int actions_horizontal_padding = design::list_item::kActionsHorizontalPadding;
    int actions_border_thickness = design::list_item::kActionsBorderThickness;
    int selected_content_stroke_thickness = design::button::kStrokeThickness;
    bool selected_content_outlined = false;
    design::TypographyRole body_role = design::TypographyRole::kBody;
    CheckboxStyle checkbox = {};
    ButtonIconStyle actions_button = {};
    uint8_t actions_background_color = design::list_item::kActionsBackgroundColor;
    uint8_t actions_border_color = design::list_item::kActionsBorderColor;
    ListItemHeaderStyle header = {};
};

UiRect ListItemBounds(int canvas_width,
                      int origin_x,
                      int origin_y,
                      const ListItemState& state,
                      const ListItemStyle& style);
void DrawListItem(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const ListItemState& state,
                  const ListItemStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_LIST_ITEM_H_
