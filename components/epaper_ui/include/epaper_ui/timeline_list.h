#ifndef EPAPER_UI_TIMELINE_LIST_H_
#define EPAPER_UI_TIMELINE_LIST_H_

#include <cstdint>
#include <string>
#include <vector>

#include "design_tokens.h"
#include "epaper_ui/list_item.h"
#include "epaper_ui/overlay_geometry.h"

struct EmbeddedImageAsset;

namespace epaper_ui {

struct TimelineGroupState {
    std::string label_text = {};
    std::vector<ListItemState> items = {};

    bool operator==(const TimelineGroupState& other) const = default;
};

// A vertical, date-grouped timeline. Each group is a label "chip" over a rail with indented item
// cards. One group can be "active" (its item list entered); focus/selection drive the visuals.
struct TimelineListState {
    std::vector<TimelineGroupState> groups = {};
    std::string empty_state_text = {};
    std::string item_label_plural = "Notes";
    const EmbeddedImageAsset* empty_state_icon_asset = nullptr;
    int visible_group_index = -1;
    int focused_group_index = -1;
    int active_group_index = -1;
    int selected_item_index = -1;

    bool operator==(const TimelineListState& other) const = default;
};

struct TimelineListStyle {
    uint8_t label_background_color = design::color::kSurfaceInverse;
    uint8_t label_text_color = design::color::kTextInverse;
    uint8_t line_color = design::color::kBorderStrong;
    uint8_t scrollbar_track_color = design::color::kScrollbarTrack;
    uint8_t scrollbar_inactive_color = design::color::kScrollbarThumb;
    uint8_t scrollbar_focused_color = design::color::kScrollbarThumbActive;
    uint8_t scrollbar_disabled_focused_color = design::color::kScrollbarThumbDisabled;
    uint8_t scrollbar_border_color = design::color::kBorderStrong;
    int width = 0;
    int height = 0;
    int label_horizontal_padding = design::timeline_list::kLabelHorizontalPadding;
    int label_vertical_padding = design::timeline_list::kLabelVerticalPadding;
    int label_border_thickness = design::timeline_list::kLabelBorderThickness;
    int label_corner_radius = design::spacing::k4;
    int line_indent = design::timeline_list::kLineIndent;
    int item_indent = design::timeline_list::kItemIndent;
    int line_thickness = design::timeline_list::kLineThickness;
    int empty_state_min_height = design::timeline_list::kEmptyStateMinHeight;
    int group_gap = design::spacing::k12;
    int max_group_height_percent = 95;
    int empty_state_gap = design::empty_state::kGap;
    int empty_state_icon_size = design::empty_state::kIconSize;
    int sticky_footer_horizontal_padding = design::spacing::k12;
    int sticky_footer_vertical_padding = design::spacing::k2;
    int sticky_footer_stroke_thickness = design::button::kStrokeThickness;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
    int scrollbar_gap = 0;
    int scrollbar_width = design::scroll_container::kScrollbarWidth;
    int scrollbar_border_thickness = design::scroll_container::kScrollbarBorderThickness;
    int min_thumb_height = design::scroll_container::kMinThumbHeight;
    uint8_t inactive_focus_ring_color = design::color::kFocusRingInactive;
    uint8_t active_focus_ring_color = design::color::kFocusRingActive;
    uint8_t focus_gap_color = design::color::kFocusGap;
    uint8_t empty_state_icon_color = design::color::kTextPrimary;
    uint8_t sticky_footer_background_color = design::color::kSurfaceRaised;
    uint8_t sticky_footer_text_color = design::color::kTextPrimary;
    design::TypographyRole label_role = design::TypographyRole::kLabelSmall;
    design::TypographyRole empty_state_role = design::TypographyRole::kLabelSmall;
    design::TypographyRole sticky_footer_role = design::TypographyRole::kLabelSmall;
    ListItemStyle item = {
        .selected_background_color = design::color::kSurfaceRaised,
        .selected_text_color = design::color::kTextPrimary,
        .selected_content_outline_color = design::color::kOutlineKnockout,
        .selected_content_stroke_thickness = design::button::kStrokeThickness,
        .selected_content_outlined = true,
        .checkbox =
            {
                .selected_background_color = design::color::kWhite,
                .selected_icon_color = design::color::kBlack,
                .selected_content_outline_color = design::color::kOutlineKnockout,
                .selected_content_stroke_thickness = design::button::kStrokeThickness,
                .selected_background_visible = true,
                .selected_content_outlined = true,
            },
        .header =
            {
                .selected_background_color = design::color::kSurfaceRaised,
                .selected_text_color = design::color::kTextPrimary,
                .selected_icon_color = design::color::kTextPrimary,
                .selected_divider_color = design::color::kTextPrimary,
                .selected_content_outlined = true,
                .selected_content_outline_color = design::color::kOutlineKnockout,
                .tag =
                    {
                        .background_color = design::color::kBlack,
                        .selected_background_color = design::color::kBlack,
                        .text_color = design::color::kWhite,
                        .selected_text_color = design::color::kWhite,
                    },
            },
    };
};

// One item's resolved on-screen rectangle, for touch resolution.
struct TimelineItemHit {
    int item_index = -1;
    UiRect bounds = {};
};

// One group's resolved geometry: the tappable chip and the visible item rows.
struct TimelineGroupHit {
    int group_index = -1;
    bool focused = false;
    bool active = false;
    UiRect chip_bounds = {};
    std::vector<TimelineItemHit> items = {};
};

// The visible portion of the timeline, produced by the same layout walk used to draw it. Callers
// use this to resolve touches to a group chip or an item row.
struct TimelineLayout {
    std::vector<TimelineGroupHit> groups = {};
};

TimelineLayout BuildTimelineLayout(int portrait_width,
                                   int portrait_height,
                                   int origin_x,
                                   int origin_y,
                                   const TimelineListState& state,
                                   const TimelineListStyle& style);

void DrawTimelineList(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      int origin_x,
                      int origin_y,
                      const TimelineListState& state,
                      const TimelineListStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_TIMELINE_LIST_H_
