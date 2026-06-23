#ifndef EPAPER_UI_NETWORK_LIST_H_
#define EPAPER_UI_NETWORK_LIST_H_

#include <cstdint>
#include <vector>

#include "design_tokens.h"
#include "epaper_ui/network_item.h"
#include "epaper_ui/overlay_geometry.h"

namespace epaper_ui {

inline constexpr int kNetworkListNoSelection = -1;

enum class NetworkListStatus : uint8_t {
    kIdle = 0,
    kNoNetworks,
    kNetworksFound,
};

struct NetworkListState {
    NetworkListStatus status = NetworkListStatus::kIdle;
    std::vector<NetworkItemState> networks = {};
    bool focused = false;
    bool active = false;
    bool focus_ring_visible = true;
    int focused_network_index = kNetworkListNoSelection;
    int selected_network_index = kNetworkListNoSelection;

    bool operator==(const NetworkListState& other) const = default;
};

struct NetworkListStyle {
    design::TypographyRole empty_state_role = design::TypographyRole::kLabelSmall;
    design::TypographyRole status_role = design::TypographyRole::kLabelSmall;
    uint8_t panel_background_color = design::color::kGrayLight;
    uint8_t panel_border_color = design::color::kBlack;
    uint8_t empty_state_text_color = design::color::kBlack;
    uint8_t empty_state_icon_color = design::color::kBlack;
    uint8_t status_text_color = design::color::kBlack;
    uint8_t inactive_focus_ring_color = design::color::kBlack;
    uint8_t active_focus_ring_color = design::color::kGrayDark;
    uint8_t focus_gap_color = design::color::kWhite;
    uint8_t scrollbar_track_color = design::color::kWhite;
    uint8_t scrollbar_inactive_color = design::color::kGrayLight;
    uint8_t scrollbar_focused_color = design::color::kBlack;
    uint8_t scrollbar_disabled_focused_color = design::color::kGrayDark;
    uint8_t scrollbar_border_color = design::color::kBlack;
    int width = 0;
    int panel_height = 0;
    int panel_border_thickness = design::network_list::kPanelBorderThickness;
    int section_gap = design::network_list::kSectionGap;
    int empty_state_gap = design::empty_state::kGap;
    int empty_state_icon_size = design::empty_state::kIconSize;
    int empty_state_stroke_thickness = design::button::kStrokeThickness;
    int focus_ring_thickness = design::toggle::kFocusRingThickness;
    int focus_gap = design::toggle::kFocusGap;
    int scrollbar_gap = design::scroll_container::kScrollbarGap;
    int scrollbar_width = design::scroll_container::kScrollbarWidth;
    int scrollbar_border_thickness = design::scroll_container::kScrollbarBorderThickness;
    int min_thumb_height = design::scroll_container::kMinThumbHeight;
    NetworkItemStyle item = {};
};

UiRect NetworkListBounds(int origin_x, int origin_y, const NetworkListStyle& style);
UiRect NetworkListPanelBounds(int origin_x, int origin_y, const NetworkListStyle& style);
UiRect NetworkListViewportBounds(int origin_x, int origin_y, const NetworkListStyle& style);
UiRect NetworkListStatusBounds(int origin_x,
                               int origin_y,
                               const NetworkListState& state,
                               const NetworkListStyle& style);
UiRect NetworkListRowBounds(int origin_x,
                            int origin_y,
                            const NetworkListState& state,
                            const NetworkListStyle& style,
                            int network_index);
bool HitTestNetworkListRow(int origin_x,
                           int origin_y,
                           const NetworkListState& state,
                           const NetworkListStyle& style,
                           int x,
                           int y,
                           int* network_index);
void DrawNetworkList(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const NetworkListState& state,
                     const NetworkListStyle& style);

}  // namespace epaper_ui

#endif  // EPAPER_UI_NETWORK_LIST_H_
