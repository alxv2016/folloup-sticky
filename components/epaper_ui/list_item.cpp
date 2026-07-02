#include "epaper_ui/list_item.h"

#include <algorithm>
#include <string>

#include "asset_types.h"
#include "epaper_ui/layout_grid.h"
#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

constexpr int kItemCornerRadius = design::spacing::k4;

template <typename DrawFn>
void ForEachOutlineOffset(int stroke_thickness, DrawFn&& fn)
{
    const int thickness = ClampPositive(stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            fn(dx, dy);
        }
    }
}

int ResolveWidth(int canvas_width, int origin_x, const ListItemStyle& style)
{
    if (style.width > 0) {
        return style.width;
    }
    return std::max(0, canvas_width - origin_x);
}

// Truncate `text` with a trailing ellipsis so it fits `max_width`.
std::string FitLabelText(design::TypographyRole role, const std::string& text, int max_width)
{
    if (text.empty() || max_width <= 0) {
        return {};
    }
    if (MeasureText(role, text) <= max_width) {
        return text;
    }
    constexpr const char* kEllipsis = "...";
    if (MeasureText(role, kEllipsis) > max_width) {
        return {};
    }
    for (size_t length = text.size(); length > 0; --length) {
        const std::string candidate = text.substr(0, length) + kEllipsis;
        if (MeasureText(role, candidate) <= max_width) {
            return candidate;
        }
    }
    return kEllipsis;
}

// Header style resolved for this row: fixed content width, item selection, and outline flags.
ListItemHeaderStyle BuildHeaderStyle(const ListItemStyle& style, int content_width)
{
    ListItemHeaderStyle header = style.header;
    header.width = content_width;
    header.selected_content_outline_color = style.selected_content_outline_color;
    header.selected_content_stroke_thickness = style.selected_content_stroke_thickness;
    header.selected_content_outlined = style.selected_content_outlined;
    return header;
}

CheckboxStyle BuildCheckboxStyle(const ListItemStyle& style)
{
    CheckboxStyle checkbox = style.checkbox;
    checkbox.selected_content_outline_color = style.selected_content_outline_color;
    checkbox.selected_content_stroke_thickness = style.selected_content_stroke_thickness;
    checkbox.selected_content_outlined = style.selected_content_outlined;
    return checkbox;
}

int ReservedAccessoryWidth(const ListItemState& state, const ListItemStyle& style)
{
    switch (state.accessory.kind) {
        case ListItemAccessoryKind::kCheckbox: {
            if (style.checkbox.size <= 0) {
                return 0;
            }
            return style.checkbox.size + ClampPositive(style.accessory_gap);
        }
        case ListItemAccessoryKind::kIcon:
            if (state.accessory.icon_asset == nullptr ||
                state.accessory.icon_asset->data == nullptr) {
                return 0;
            }
            return ClampPositive(style.checkbox.size) + ClampPositive(style.accessory_gap);
        case ListItemAccessoryKind::kNone:
        default:
            return 0;
    }
}

LayoutGrid BuildGrid(int canvas_width, const ListItemState& state, int origin_x, int origin_y,
                     const ListItemStyle& style)
{
    const int width = ResolveWidth(canvas_width, origin_x, style);
    const int accessory_width = ReservedAccessoryWidth(state, style);
    const int content_width = std::max(0, width - ClampPositive(style.padding_left) -
                                              accessory_width - ClampPositive(style.padding_right));
    const ListItemHeaderState header_state = {
        .icon_asset = state.header.icon_asset,
        .tag_icon_asset = state.header.tag_icon_asset,
        .time_text = state.header.time_text,
        .minute_seconds_text = state.header.minute_seconds_text,
        .tag_text = state.header.tag_text,
        .selected = state.selected,
    };
    const int header_height =
        ListItemHeaderBounds(0, 0, header_state, BuildHeaderStyle(style, content_width)).height;
    const int body_height = LineHeight(style.body_role);
    const int content_height = ClampPositive(style.padding_top) + header_height +
                               ClampPositive(style.row_gap) + body_height +
                               ClampPositive(style.padding_bottom);
    int min_height = content_height;
    if (state.accessory.kind == ListItemAccessoryKind::kCheckbox && style.checkbox.size > 0) {
        min_height = std::max(min_height, ClampPositive(style.padding_top) + style.checkbox.size +
                                              ClampPositive(style.padding_bottom));
    } else if (state.accessory.kind == ListItemAccessoryKind::kIcon) {
        min_height = std::max(min_height, ClampPositive(style.padding_top) +
                                              ClampPositive(style.checkbox.size) +
                                              ClampPositive(style.padding_bottom));
    }

    LayoutGrid grid = {};
    grid.origin_x = origin_x;
    grid.origin_y = origin_y;
    grid.style.width = width;
    grid.style.min_height = min_height;
    grid.style.column_count = 1;
    grid.style.row_count = 2;
    grid.style.padding_left = ClampPositive(style.padding_left) + accessory_width;
    grid.style.padding_right = ClampPositive(style.padding_right);
    grid.style.padding_top = ClampPositive(style.padding_top);
    grid.style.padding_bottom = ClampPositive(style.padding_bottom);
    grid.style.column_gap = 0;
    grid.style.row_gap = ClampPositive(style.row_gap);
    grid.style.fixed_row_side = FixedRowSide::kTop;
    grid.style.fixed_row_height = header_height;
    return grid;
}

UiRect ActionTrayBounds(const UiRect& item_bounds, const ListItemStyle& style)
{
    const UiRect button = ButtonIconBounds(0, 0, style.actions_button);
    const int horizontal_padding = ClampPositive(style.actions_horizontal_padding);
    const int gap = ClampPositive(style.actions_gap);
    const int overlay_width = (2 * horizontal_padding) + (2 * button.width) + gap;
    const int overlay_height = std::max(item_bounds.height, button.height);
    const int clamped_width = std::min(item_bounds.width, overlay_width);
    return {item_bounds.right() - clamped_width, item_bounds.y, clamped_width, overlay_height};
}

}  // namespace

UiRect ListItemBounds(int canvas_width,
                      int origin_x,
                      int origin_y,
                      const ListItemState& state,
                      const ListItemStyle& style)
{
    return BuildGrid(canvas_width, state, origin_x, origin_y, style).Measure(canvas_width);
}

void DrawListItem(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int origin_x,
                  int origin_y,
                  const ListItemState& state,
                  const ListItemStyle& style)
{
    if (style.width < 0) {
        return;
    }

    const LayoutGrid grid = BuildGrid(portrait_width, state, origin_x, origin_y, style);
    const UiRect bounds = grid.Measure(portrait_width);
    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t body_color = state.selected ? style.selected_text_color : style.text_color;
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            bounds, kItemCornerRadius, background_color);

    const int border_height = ClampPositive(style.bottom_border_thickness);
    if (border_height > 0) {
        const int separator_inset = std::min(kItemCornerRadius, bounds.width / 2);
        const int separator_width = std::max(0, bounds.width - (2 * separator_inset));
        if (separator_width > 0) {
            const UiRect separator = {bounds.x + separator_inset,
                                      bounds.bottom() - std::min(border_height, bounds.height),
                                      separator_width, std::min(border_height, bounds.height)};
            FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                             separator, style.border_color);
        }
    }

    const UiRect header_cell = grid.CellBounds(portrait_width, 0, 0);
    if (state.accessory.kind == ListItemAccessoryKind::kCheckbox && style.checkbox.size > 0) {
        const CheckboxStyle checkbox_style = BuildCheckboxStyle(style);
        const int checkbox_y = header_cell.y +
                               std::max(0, (header_cell.height - style.checkbox.size) / 2);
        const CheckboxState checkbox_state = {.checked = state.accessory.checked,
                                              .selected = state.selected};
        DrawCheckbox(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     bounds.x + ClampPositive(style.padding_left), checkbox_y, checkbox_state,
                     checkbox_style);
    } else if (state.accessory.kind == ListItemAccessoryKind::kIcon &&
               state.accessory.icon_asset != nullptr &&
               state.accessory.icon_asset->data != nullptr) {
        const int slot_size = ClampPositive(style.checkbox.size);
        const int draw_width =
            std::min(slot_size, static_cast<int>(state.accessory.icon_asset->width));
        const int draw_height =
            std::min(slot_size, static_cast<int>(state.accessory.icon_asset->height));
        const uint8_t icon_color =
            state.selected ? style.checkbox.selected_icon_color : style.checkbox.icon_color;
        const int icon_x =
            bounds.x + ClampPositive(style.padding_left) + CenterOffset(slot_size, draw_width);
        const int icon_y = header_cell.y + CenterOffset(header_cell.height, draw_height);
        if (state.selected && style.selected_content_outlined) {
            ForEachOutlineOffset(style.selected_content_stroke_thickness, [&](int dx, int dy) {
                DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width,
                                      portrait_height, icon_x + dx, icon_y + dy,
                                      state.accessory.icon_asset,
                                      style.selected_content_outline_color);
            });
        }
        DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                              icon_x, icon_y, state.accessory.icon_asset, icon_color);
    }

    const ListItemHeaderState header_state = {
        .icon_asset = state.header.icon_asset,
        .tag_icon_asset = state.header.tag_icon_asset,
        .time_text = state.header.time_text,
        .minute_seconds_text = state.header.minute_seconds_text,
        .tag_text = state.header.tag_text,
        .selected = state.selected,
    };
    DrawListItemHeader(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       header_cell.x, header_cell.y, header_state,
                       BuildHeaderStyle(style, header_cell.width));

    if (!state.body_text.empty()) {
        const UiRect body_cell = grid.CellBounds(portrait_width, 0, 1);
        const std::string body_text = FitLabelText(style.body_role, state.body_text, body_cell.width);
        if (!body_text.empty()) {
            if (state.selected && style.selected_content_outlined) {
                ForEachOutlineOffset(style.selected_content_stroke_thickness, [&](int dx, int dy) {
                    DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width,
                                       portrait_height, body_cell.x + dx, body_cell.y + dy,
                                       body_text, style.body_role,
                                       style.selected_content_outline_color);
                });
            }
            DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                               body_cell.x, body_cell.y, body_text, style.body_role, body_color);
        }
    }

    if (!state.actions.visible) {
        return;
    }

    const UiRect action_bounds = ActionTrayBounds(bounds, style);
    FillPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                     action_bounds, style.actions_background_color);
    DrawPortraitBorder(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                       action_bounds, ClampPositive(style.actions_border_thickness),
                       style.actions_border_color);

    const UiRect button = ButtonIconBounds(0, 0, style.actions_button);
    const int button_y = action_bounds.y + std::max(0, (action_bounds.height - button.height) / 2);
    const int left_x = action_bounds.x + ClampPositive(style.actions_horizontal_padding);
    const int right_x = left_x + button.width + ClampPositive(style.actions_gap);

    const ButtonIconState close_state = {.asset = project_assets::GetIcon(EmbeddedIconId::kClose),
                                         .selected = state.actions.selected_action_index == 0};
    DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height, left_x,
                   button_y, close_state, style.actions_button);
    const ButtonIconState confirm_state = {.asset = project_assets::GetIcon(EmbeddedIconId::kCheck),
                                           .selected = state.actions.selected_action_index == 1};
    DrawButtonIcon(framebuffer, raw_width, raw_height, portrait_width, portrait_height, right_x,
                   button_y, confirm_state, style.actions_button);
}

}  // namespace epaper_ui
