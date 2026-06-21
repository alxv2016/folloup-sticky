#include "epaper_ui/toast.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/font_renderer.h"
#include "project_assets.h"

namespace epaper_ui {
namespace {

constexpr design::TypographyRole kToastRole = design::TypographyRole::kBody;

void DrawRawPixel(uint8_t* framebuffer, int raw_width, int raw_height, int x, int y, bool black)
{
    if (framebuffer == nullptr || x < 0 || y < 0 || x >= raw_width || y >= raw_height) {
        return;
    }

    const size_t index = static_cast<size_t>(y) * static_cast<size_t>(raw_width / 8) +
                         static_cast<size_t>(x / 8);
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x07));
    if (black) {
        framebuffer[index] &= static_cast<uint8_t>(~mask);
    } else {
        framebuffer[index] |= mask;
    }
}

void DrawPortraitPixel(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int x,
                       int y,
                       bool black)
{
    if (x < 0 || y < 0 || x >= portrait_width || y >= portrait_height) {
        return;
    }

    const int raw_x = y;
    const int raw_y = raw_height - 1 - x;
    DrawRawPixel(framebuffer, raw_width, raw_height, raw_x, raw_y, black);
}

bool ShouldDrawBlackForTone(int x, int y, uint8_t tone)
{
    if (tone <= design::color::kGray1) {
        return true;
    }
    if (tone <= design::color::kGray2) {
        return ((x + y) & 1) == 0;
    }
    if (tone < design::color::kGray4) {
        return (x % 2 == 0) && (y % 2 == 0);
    }
    return false;
}

void FillPortraitRect(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      const UiRect& rect,
                      uint8_t tone)
{
    if (rect.IsEmpty()) {
        return;
    }

    for (int row = 0; row < rect.height; ++row) {
        for (int col = 0; col < rect.width; ++col) {
            const int px = rect.x + col;
            const int py = rect.y + row;
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

void DrawPortraitBorder(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        const UiRect& rect,
                        int thickness,
                        uint8_t tone)
{
    if (rect.IsEmpty() || thickness <= 0) {
        return;
    }

    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x, rect.y, rect.width, std::min(thickness, rect.height)},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x,
                      rect.bottom() - std::min(thickness, rect.height),
                      rect.width,
                      std::min(thickness, rect.height)},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.x, rect.y, std::min(thickness, rect.width), rect.height},
                     tone);
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     {rect.right() - std::min(thickness, rect.width),
                      rect.y,
                      std::min(thickness, rect.width),
                      rect.height},
                     tone);
}

void DrawTextLine(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int x,
                  int y,
                  std::string_view text,
                  uint8_t tone)
{
    epaper_ui::DrawText(
        [&](int px, int py, uint8_t color) {
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              px,
                              py,
                              ShouldDrawBlackForTone(px, py, color));
        },
        x,
        y,
        text,
        tone,
        kToastRole);
}

bool AssetPixelSet(const EmbeddedImageAsset& asset, int x, int y)
{
    if (asset.data == nullptr || x < 0 || y < 0 || x >= asset.width || y >= asset.height) {
        return false;
    }

    const size_t byte_index =
        static_cast<size_t>(y) * asset.stride_bytes + static_cast<size_t>(x / 8);
    const uint8_t bit_mask = static_cast<uint8_t>(0x80U >> (x & 0x07));
    return (asset.data[byte_index] & bit_mask) != 0;
}

void DrawPortraitMonoAsset(uint8_t* framebuffer,
                           int raw_width,
                           int raw_height,
                           int portrait_width,
                           int portrait_height,
                           int x,
                           int y,
                           const EmbeddedImageAsset* asset,
                           uint8_t tone)
{
    if (framebuffer == nullptr || asset == nullptr || asset->format != ImageFormat::kMono1) {
        return;
    }

    for (int row = 0; row < asset->height; ++row) {
        for (int col = 0; col < asset->width; ++col) {
            if (!AssetPixelSet(*asset, col, row)) {
                continue;
            }
            DrawPortraitPixel(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              x + col,
                              y + row,
                              ShouldDrawBlackForTone(x + col, y + row, tone));
        }
    }
}

const EmbeddedImageAsset* ResolveCloseIcon()
{
    return project_assets::GetIcon(EmbeddedIconId::kClose);
}

std::array<std::string_view, 4> WrapLines(std::string_view text, int max_width, int* line_count)
{
    std::array<std::string_view, 4> lines = {};
    *line_count = 0;
    if (text.empty() || max_width <= 0) {
        return lines;
    }

    size_t line_start = 0;
    while (line_start < text.size() && *line_count < static_cast<int>(lines.size())) {
        while (line_start < text.size() && text[line_start] == ' ') {
            ++line_start;
        }
        if (line_start >= text.size()) {
            break;
        }

        size_t segment_end = line_start;
        size_t last_fit_end = line_start;
        while (segment_end < text.size()) {
            size_t next_space = text.find(' ', segment_end);
            if (next_space == std::string_view::npos) {
                next_space = text.size();
            }

            const std::string_view candidate = text.substr(line_start, next_space - line_start);
            if (MeasureText(kToastRole, candidate) > max_width) {
                break;
            }

            last_fit_end = next_space;
            if (next_space == text.size()) {
                segment_end = next_space;
                break;
            }
            segment_end = next_space + 1;
        }

        if (last_fit_end == line_start) {
            last_fit_end = text.size();
            for (size_t split = line_start + 1; split <= text.size(); ++split) {
                if (MeasureText(kToastRole, text.substr(line_start, split - line_start)) >
                    max_width) {
                    last_fit_end = split - 1;
                    break;
                }
            }
            if (last_fit_end <= line_start) {
                last_fit_end = std::min(text.size(), line_start + 1);
            }
        }

        lines[static_cast<size_t>(*line_count)] =
            text.substr(line_start, last_fit_end - line_start);
        ++(*line_count);
        line_start = last_fit_end;
        while (line_start < text.size() && text[line_start] == ' ') {
            ++line_start;
        }
    }

    return lines;
}

UiRect PanelBounds(int portrait_width, int portrait_height, const ToastState& state)
{
    if (!state.visible || state.body_text.empty()) {
        return {};
    }

    const int width = std::max(0, portrait_width - (2 * design::toast::kScreenInset));
    const int leading_width =
        state.leading_icon != nullptr ? design::toast::kIconSize + design::toast::kIconGap : 0;
    const int close_width =
        state.show_close_button ? design::toast::kIconSize + design::toast::kTextCloseGap : 0;
    const int text_width = std::max(
        0,
        width - (2 * design::toast::kHorizontalPadding) - leading_width - close_width);
    int line_count = 0;
    (void)WrapLines(state.body_text, text_width, &line_count);
    const int content_height =
        std::max(design::toast::kIconSize, line_count * LineHeight(kToastRole));
    const int height = (2 * design::toast::kVerticalPadding) + content_height;
    return {(portrait_width - width) / 2,
            std::max(design::status_bar::kHeight + design::spacing::k16,
                     portrait_height - design::toast::kBottomInset - height),
            width,
            height};
}

}  // namespace

UiRect ToastPanelBounds(int portrait_width, int portrait_height, const ToastState& state)
{
    return PanelBounds(portrait_width, portrait_height, state);
}

UiRect ToastCloseButtonBounds(int portrait_width, int portrait_height, const ToastState& state)
{
    const UiRect panel = PanelBounds(portrait_width, portrait_height, state);
    const EmbeddedImageAsset* close_icon = ResolveCloseIcon();
    if (panel.IsEmpty() || !state.show_close_button || close_icon == nullptr) {
        return {};
    }

    return {panel.right() - design::toast::kHorizontalPadding - design::toast::kIconSize,
            panel.y + std::max(0, (panel.height - design::toast::kIconSize) / 2),
            design::toast::kIconSize,
            design::toast::kIconSize};
}

bool HitTestToastCloseButton(int portrait_width,
                             int portrait_height,
                             const ToastState& state,
                             int x,
                             int y)
{
    return ToastCloseButtonBounds(portrait_width, portrait_height, state).Contains(x, y);
}

void DrawToast(uint8_t* framebuffer,
               int raw_width,
               int raw_height,
               int portrait_width,
               int portrait_height,
               const ToastState& state)
{
    const UiRect panel = PanelBounds(portrait_width, portrait_height, state);
    if (panel.IsEmpty() || framebuffer == nullptr) {
        return;
    }

    const UiRect shadow = {panel.x + design::toast::kShadowOffset,
                           panel.y + design::toast::kShadowOffset,
                           panel.width,
                           panel.height};
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     shadow,
                     design::color::kShadow);
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
                       design::toast::kBorderThickness,
                       design::color::kBlack);

    const UiRect close_bounds = ToastCloseButtonBounds(portrait_width, portrait_height, state);
    int text_x = panel.x + design::toast::kHorizontalPadding;
    if (state.leading_icon != nullptr) {
        const int icon_y = panel.y + std::max(0, (panel.height - design::toast::kIconSize) / 2);
        DrawPortraitMonoAsset(framebuffer,
                              raw_width,
                              raw_height,
                              portrait_width,
                              portrait_height,
                              text_x,
                              icon_y,
                              state.leading_icon,
                              design::color::kBlack);
        text_x += design::toast::kIconSize + design::toast::kIconGap;
    }

    const int text_right = close_bounds.IsEmpty()
                               ? panel.right() - design::toast::kHorizontalPadding
                               : close_bounds.x - design::toast::kTextCloseGap;
    const int text_width = std::max(0, text_right - text_x);
    int line_count = 0;
    const auto lines = WrapLines(state.body_text, text_width, &line_count);
    int text_y = panel.y + design::toast::kVerticalPadding;
    for (int index = 0; index < line_count; ++index) {
        DrawTextLine(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     text_x,
                     text_y,
                     lines[static_cast<size_t>(index)],
                     design::color::kBlack);
        text_y += LineHeight(kToastRole);
    }

    if (!close_bounds.IsEmpty()) {
        const uint8_t fill =
            state.close_button_focused ? design::color::kBlack : design::color::kWhite;
        const uint8_t icon_tone =
            state.close_button_focused ? design::color::kWhite : design::color::kBlack;
        FillPortraitRect(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         close_bounds,
                         fill);
        DrawPortraitBorder(framebuffer,
                           raw_width,
                           raw_height,
                           portrait_width,
                           portrait_height,
                           close_bounds,
                           design::button::kBorderThickness,
                           design::color::kBlack);
        const EmbeddedImageAsset* close_icon = ResolveCloseIcon();
        if (close_icon != nullptr) {
            const int icon_x =
                close_bounds.x + std::max(0, (close_bounds.width - close_icon->width) / 2);
            const int icon_y =
                close_bounds.y + std::max(0, (close_bounds.height - close_icon->height) / 2);
            DrawPortraitMonoAsset(framebuffer,
                                  raw_width,
                                  raw_height,
                                  portrait_width,
                                  portrait_height,
                                  icon_x,
                                  icon_y,
                                  close_icon,
                                  icon_tone);
        }
    }
}

}  // namespace epaper_ui
