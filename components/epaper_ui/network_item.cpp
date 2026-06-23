#include "epaper_ui/network_item.h"

#include <algorithm>
#include <array>

#include "project_assets.h"
#include "render_utils.h"

namespace epaper_ui {
namespace {

std::string FitLabelText(std::string_view text, design::TypographyRole role, int max_width)
{
    if (text.empty() || max_width <= 0) {
        return {};
    }
    if (MeasureText(role, text) <= max_width) {
        return std::string(text);
    }

    constexpr std::string_view kEllipsis = "...";
    if (MeasureText(role, kEllipsis) > max_width) {
        return {};
    }

    size_t length = text.size();
    while (length > 0) {
        std::string candidate = std::string(text.substr(0, length)) + std::string(kEllipsis);
        if (MeasureText(role, candidate) <= max_width) {
            return candidate;
        }
        --length;
    }
    return std::string(kEllipsis);
}

const EmbeddedImageAsset* ResolveWifiAsset(NetworkSignalStrength strength)
{
    return project_assets::GetIcon(strength == NetworkSignalStrength::kMedium
                                       ? EmbeddedIconId::kWbar2
                                       : EmbeddedIconId::kWbar3);
}

template <typename DrawFn>
void DrawOutlined(int origin_x, int origin_y, int stroke_thickness, DrawFn&& draw_fn)
{
    const int thickness = ClampPositive(stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            draw_fn(origin_x + dx, origin_y + dy);
        }
    }
}

}  // namespace

UiRect NetworkItemBounds(int origin_x, int origin_y, const NetworkItemStyle& style)
{
    return {origin_x, origin_y, ClampPositive(style.width), ClampPositive(style.height)};
}

void DrawNetworkItem(uint8_t* framebuffer,
                     int raw_width,
                     int raw_height,
                     int portrait_width,
                     int portrait_height,
                     int origin_x,
                     int origin_y,
                     const NetworkItemState& state,
                     const NetworkItemStyle& style)
{
    const UiRect bounds = NetworkItemBounds(origin_x, origin_y, style);
    if (bounds.IsEmpty()) {
        return;
    }

    const uint8_t background = state.selected ? style.selected_background_color
                                              : style.background_color;
    const uint8_t text_color = state.selected ? style.selected_text_color : style.text_color;
    const uint8_t icon_color = state.selected ? style.selected_icon_color : style.icon_color;
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     bounds,
                     background);

    const int border_height = ClampPositive(style.bottom_border_thickness);
    if (border_height > 0) {
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         {bounds.x,
                          bounds.bottom() - std::min(border_height, bounds.height),
                          bounds.width,
                          std::min(border_height, bounds.height)},
                         style.border_color);
    }

    const std::array<const EmbeddedImageAsset*, 3> assets = {
        state.current_network ? project_assets::GetIcon(EmbeddedIconId::kCheck) : nullptr,
        state.private_network ? project_assets::GetIcon(EmbeddedIconId::kLock) : nullptr,
        ResolveWifiAsset(state.signal_strength),
    };

    const int icon_gap = ClampPositive(style.icon_gap);
    const int slot_size = ClampPositive(style.icon_size);
    int next_icon_x = bounds.right() - ClampPositive(style.horizontal_padding);
    int visible_icon_count = 0;
    for (const EmbeddedImageAsset* asset : assets) {
        if (asset != nullptr) {
            ++visible_icon_count;
        }
    }

    for (int index = static_cast<int>(assets.size()) - 1; index >= 0; --index) {
        const EmbeddedImageAsset* asset = assets[static_cast<size_t>(index)];
        if (asset == nullptr) {
            continue;
        }

        const int slot_width = std::max(slot_size, static_cast<int>(asset->width));
        next_icon_x -= slot_width;
        const int icon_x =
            next_icon_x + CenterOffset(slot_width, static_cast<int>(asset->width));
        const int icon_y =
            bounds.y + CenterOffset(bounds.height, static_cast<int>(asset->height));
        if (state.selected && style.selected_content_outlined) {
            DrawOutlined(icon_x, icon_y, style.selected_content_stroke_thickness, [&](int x, int y) {
                DrawPortraitMonoAsset(framebuffer,
                                      raw_width,
                                      raw_height,
                                      portrait_width,
                                      portrait_height,
                                      x,
                                      y,
                                      asset,
                                      style.selected_content_outline_color);
            });
        }
        DrawPortraitMonoAsset(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              icon_x,
                              icon_y,
                              asset,
                              icon_color);
        next_icon_x -= icon_gap;
    }

    if (state.ssid_text.empty()) {
        return;
    }

    const int label_x = bounds.x + ClampPositive(style.horizontal_padding);
    const int label_max_right =
        visible_icon_count > 0 ? next_icon_x : bounds.right() - ClampPositive(style.horizontal_padding);
    const int available_width = std::max(0, label_max_right - label_x);
    const std::string text = FitLabelText(state.ssid_text, style.role, available_width);
    if (text.empty()) {
        return;
    }

    const int text_y = bounds.y + CenterOffset(bounds.height, LineHeight(style.role));
    if (state.selected && style.selected_content_outlined) {
        DrawOutlined(label_x,
                     text_y,
                     style.selected_content_stroke_thickness,
                     [&](int x, int y) {
                         DrawTypographyText(framebuffer,
                                            raw_width,
                                            raw_height,
                                            portrait_width,
                                            portrait_height,
                                            x,
                                            y,
                                            text,
                                            style.role,
                                            style.selected_content_outline_color);
                     });
    }
    DrawTypographyText(framebuffer,
                       raw_width,
                       raw_height,
                       portrait_width,
                       portrait_height,
                       label_x,
                       text_y,
                       text,
                       style.role,
                       text_color);
}

}  // namespace epaper_ui
