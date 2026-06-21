#include "epaper_ui/select_item.h"

#include <algorithm>
#include <string>

#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

std::string FitLabelText(const std::string& text, design::TypographyRole role, int max_width)
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

    size_t length = text.size();
    while (length > 0) {
        const std::string candidate = text.substr(0, length) + kEllipsis;
        if (MeasureText(role, candidate) <= max_width) {
            return candidate;
        }
        --length;
    }
    return kEllipsis;
}

}  // namespace

UiRect SelectItemBounds(int origin_x, int origin_y, const SelectItemStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.height)};
}

void DrawSelectItem(uint8_t* framebuffer,
                    int raw_width,
                    int raw_height,
                    int portrait_width,
                    int portrait_height,
                    int origin_x,
                    int origin_y,
                    const SelectItemState& state,
                    const SelectItemStyle& style)
{
    const UiRect bounds = SelectItemBounds(origin_x, origin_y, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const uint8_t background_color =
        state.selected ? style.selected_background_color : style.background_color;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;
    const uint8_t icon_color = state.selected ? style.selected_icon_color : style.icon_color;

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     bounds,
                     background_color);

    if (style.bottom_border_thickness > 0) {
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         {bounds.x,
                          bounds.bottom() - std::min(style.bottom_border_thickness, bounds.height),
                          bounds.width,
                          std::min(style.bottom_border_thickness, bounds.height)},
                         style.border_color);
    }

    const EmbeddedImageAsset* check_asset =
        state.checked ? project_assets::GetIcon(EmbeddedIconId::kCheck) : nullptr;

    int label_max_right = bounds.right() - ClampPositive(style.horizontal_padding);
    if (check_asset != nullptr) {
        const int icon_slot_size = ClampPositive(style.icon_size);
        const int slot_width = std::max(icon_slot_size, static_cast<int>(check_asset->width));
        const int slot_x = bounds.right() - ClampPositive(style.horizontal_padding) - slot_width;
        DrawPortraitMonoAsset(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              slot_x + CenterOffset(slot_width, check_asset->width),
                              bounds.y + CenterOffset(bounds.height, check_asset->height),
                              check_asset,
                              icon_color);
        label_max_right = slot_x;
    }

    const int label_x = bounds.x + ClampPositive(style.horizontal_padding);
    const int available_label_width = std::max(0, label_max_right - label_x);
    const std::string fitted_label = FitLabelText(state.label_text, style.role, available_label_width);
    if (fitted_label.empty()) {
        return;
    }

    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       label_x,
                       bounds.y + CenterOffset(bounds.height, LineHeight(style.role)),
                       fitted_label,
                       style.role,
                       text_color);
}

}  // namespace epaper_ui
