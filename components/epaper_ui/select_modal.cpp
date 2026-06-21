#include "epaper_ui/select_modal.h"

#include <algorithm>

#include "design_tokens.h"
#include "epaper_ui/select_list.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr auto kTitleRole = design::TypographyRole::kHeadingH2;
constexpr int kMaxVisibleRows = 6;
constexpr int kHorizontalPadding = design::spacing::k12;
constexpr int kVerticalPadding = design::spacing::k12;
constexpr int kTitleGap = design::spacing::k8;

int TitleHeight(const SelectModalState& state)
{
    return state.title_text.empty() ? 0 : LineHeight(kTitleRole);
}

int VisibleRowCount(const SelectModalState& state)
{
    return std::min(static_cast<int>(state.items.size()), kMaxVisibleRows);
}

SelectListStyle BuildListStyle(int width, int visible_row_count)
{
    SelectListStyle style = {};
    style.width = width;
    style.panel_height = (2 * design::network_list::kPanelBorderThickness) +
                         (visible_row_count * design::network_item::kHeight);
    style.focus_ring_thickness = 0;
    style.focus_gap = 0;
    style.item.role = design::TypographyRole::kLabelMedium;
    style.item.height = design::network_item::kHeight;
    style.item.horizontal_padding = design::network_item::kHorizontalPadding;
    style.item.icon_size = design::network_item::kIconSize;
    style.item.bottom_border_thickness = design::network_item::kBottomBorderThickness;
    return style;
}

SelectListState BuildListState(const SelectModalState& state)
{
    SelectListState list_state = {};
    list_state.focused = false;
    list_state.active = true;
    list_state.selected_item_index = state.selected_index;
    list_state.items.reserve(state.items.size());
    for (const SelectModalItemState& item : state.items) {
        list_state.items.push_back({.label_text = item.label_text});
    }
    return list_state;
}

UiRect ListPanelBounds(int portrait_width, int portrait_height, const SelectModalState& state)
{
    const UiRect panel = SelectModalPanelBounds(portrait_width, portrait_height, state);
    if (panel.IsEmpty()) {
        return {};
    }

    const int title_height = TitleHeight(state);
    const int title_gap = title_height > 0 && !state.items.empty() ? kTitleGap : 0;
    const int list_x = panel.x + kHorizontalPadding;
    const int list_width = std::max(0, panel.width - (2 * kHorizontalPadding));
    const int list_y = panel.y + kVerticalPadding + title_height + title_gap;
    const SelectListStyle list_style = BuildListStyle(list_width, VisibleRowCount(state));
    return SelectListPanelBounds(list_x, list_y, list_style);
}

}  // namespace

UiRect SelectModalPanelBounds(int portrait_width,
                              int portrait_height,
                              const SelectModalState& state)
{
    if (!state.visible || state.items.empty()) {
        return {};
    }

    const int width = std::max(0, portrait_width - (2 * design::modal::kScreenInset));
    const int title_height = TitleHeight(state);
    const int title_gap = title_height > 0 ? kTitleGap : 0;
    const int body_height = BuildListStyle(width - (2 * kHorizontalPadding), VisibleRowCount(state))
                                .panel_height;
    const int height = (2 * kVerticalPadding) + title_height + title_gap + body_height;
    return {design::modal::kScreenInset,
            std::max(0, (portrait_height - height) / 2),
            width,
            height};
}

UiRect SelectModalItemBounds(int portrait_width,
                             int portrait_height,
                             const SelectModalState& state,
                             int index)
{
    if (index < 0 || index >= static_cast<int>(state.items.size())) {
        return {};
    }

    const UiRect list_panel = ListPanelBounds(portrait_width, portrait_height, state);
    if (list_panel.IsEmpty()) {
        return {};
    }

    const SelectListState list_state = BuildListState(state);
    const SelectListStyle list_style = BuildListStyle(list_panel.width, VisibleRowCount(state));
    return SelectListItemBounds(list_panel.x, list_panel.y, list_state, list_style, index);
}

int HitTestSelectModalItem(int portrait_width,
                           int portrait_height,
                           const SelectModalState& state,
                           int x,
                           int y,
                           bool* hit)
{
    if (hit != nullptr) {
        *hit = false;
    }

    for (int index = 0; index < static_cast<int>(state.items.size()); ++index) {
        const UiRect bounds = SelectModalItemBounds(portrait_width, portrait_height, state, index);
        if (!bounds.IsEmpty() && bounds.Contains(x, y)) {
            if (hit != nullptr) {
                *hit = true;
            }
            return index;
        }
    }

    return -1;
}

void DrawSelectModal(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     const SelectModalState& state)
{
    const UiRect panel = SelectModalPanelBounds(portrait_width, portrait_height, state);
    if (panel.IsEmpty()) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {panel.x + design::modal::kShadowOffset,
                      panel.y + design::modal::kShadowOffset,
                      panel.width,
                      panel.height},
                     design::color::kGrayMid);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     panel,
                     design::color::kWhite);
    DrawPortraitBorder(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       panel,
                       design::modal::kBorderThickness,
                       design::color::kBlack);

    if (!state.title_text.empty()) {
        DrawTypographyText(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           panel.x + kHorizontalPadding,
                           panel.y + kVerticalPadding,
                           state.title_text,
                           kTitleRole,
                           design::color::kBlack);
    }

    const UiRect list_panel = ListPanelBounds(portrait_width, portrait_height, state);
    if (list_panel.IsEmpty()) {
        return;
    }

    const SelectListState list_state = BuildListState(state);
    const SelectListStyle list_style = BuildListStyle(list_panel.width, VisibleRowCount(state));
    DrawSelectList(framebuffer,
                   raw_width,
                   raw_height,
                   portrait_width,
                   portrait_height,
                   list_panel.x,
                   list_panel.y,
                   list_state,
                   list_style);
}

}  // namespace epaper_ui
