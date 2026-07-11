#include "epaper_ui/completion_banner.h"

#include <algorithm>
#include <vector>

#include "render_utils.h"

namespace epaper_ui {
namespace {

int TextStartX(int origin_x, const CompletionBannerStyle& style)
{
    return origin_x + ClampPositive(style.padding) + ClampPositive(style.icon_container_size) +
           ClampPositive(style.content_gap);
}

int TextMaxWidth(int origin_x, const CompletionBannerStyle& style)
{
    const int width = ClampPositive(style.width);
    const int text_x = TextStartX(origin_x, style);
    return std::max(0, (origin_x + width) - ClampPositive(style.padding) - text_x);
}

}  // namespace

UiRect CompletionBannerBounds(int origin_x,
                              int origin_y,
                              const CompletionBannerState& state,
                              const CompletionBannerStyle& style)
{
    const int padding = ClampPositive(style.padding);
    const int icon_container = ClampPositive(style.icon_container_size);
    const std::vector<std::string> lines =
        WrapTextToWidth(style.role, state.message_text, TextMaxWidth(origin_x, style));
    const int text_height =
        lines.empty() ? 0 : static_cast<int>(lines.size()) * std::max(1, LineHeight(style.role));
    const int height =
        std::max(ClampPositive(style.min_height), std::max(icon_container, text_height) +
                                                      (2 * padding));
    return {origin_x, origin_y, ClampPositive(style.width), height};
}

void DrawCompletionBanner(uint8_t* framebuffer,
                          int raw_width,
                          int raw_height,
                          int portrait_width,
                          int portrait_height,
                          int origin_x,
                          int origin_y,
                          const CompletionBannerState& state,
                          const CompletionBannerStyle& style)
{
    const UiRect bounds = CompletionBannerBounds(origin_x, origin_y, state, style);
    if (bounds.IsEmpty()) {
        return;
    }

    // Asymmetric background: a full-height circle forms the fully rounded left corners (a rounded
    // rect whose radius is half its size is a circle), then a right-rounded rect fills the rest.
    const int left_radius = std::max(0, std::min(ClampPositive(style.left_corner_radius),
                                                 bounds.height / 2));
    const int right_radius = std::max(0, std::min(ClampPositive(style.right_corner_radius),
                                                  bounds.height / 2));
    const UiRect left_cap = {bounds.x, bounds.y, 2 * left_radius, bounds.height};
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            left_cap, left_radius, style.background_color);
    const UiRect right_body = {bounds.x + left_radius, bounds.y,
                               std::max(0, bounds.width - left_radius), bounds.height};
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            right_body, right_radius, style.background_color);

    const int padding = ClampPositive(style.padding);
    const int icon_container = ClampPositive(style.icon_container_size);
    const UiRect icon_bounds = {bounds.x + padding,
                                bounds.y + CenterOffset(bounds.height, icon_container),
                                icon_container, icon_container};
    FillRoundedPortraitRect(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                            icon_bounds, icon_container / 2, style.icon_background_color);

    const EmbeddedImageAsset* asset = project_assets::GetIcon(state.icon);
    if (asset != nullptr) {
        DrawPortraitMonoAsset(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                              icon_bounds.x + CenterOffset(icon_bounds.width, asset->width),
                              icon_bounds.y + CenterOffset(icon_bounds.height, asset->height),
                              asset, style.icon_color);
    }

    const std::vector<std::string> lines =
        WrapTextToWidth(style.role, state.message_text, TextMaxWidth(origin_x, style));
    if (lines.empty()) {
        return;
    }

    const int line_height = std::max(1, LineHeight(style.role));
    const int text_x = TextStartX(origin_x, style);
    int cursor_y =
        bounds.y + CenterOffset(bounds.height, static_cast<int>(lines.size()) * line_height);
    for (const std::string& line : lines) {
        DrawTypographyText(framebuffer, raw_width, raw_height, portrait_width, portrait_height,
                           text_x, cursor_y, line, style.role, style.text_color);
        cursor_y += line_height;
    }
}

}  // namespace epaper_ui
