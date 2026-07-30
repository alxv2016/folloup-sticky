#include "render_utils.h"

#include <algorithm>
#include <cstddef>

namespace epaper_ui {

namespace {

void DrawRawPixel(uint8_t* framebuffer, int raw_width, int raw_height, int x, int y, bool black)
{
    if (framebuffer == nullptr || x < 0 || y < 0 || x >= raw_width || y >= raw_height) {
        return;
    }

    const size_t bytes_per_row = static_cast<size_t>(raw_width / 8);
    const size_t index = static_cast<size_t>(y) * bytes_per_row + static_cast<size_t>(x / 8);
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x07));
    if (black) {
        framebuffer[index] &= static_cast<uint8_t>(~mask);
    } else {
        framebuffer[index] |= mask;
    }
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

bool RoundedRectContains(int width, int height, int radius, int x, int y)
{
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }

    const int clamped_radius = std::max(0, std::min(radius, std::min(width, height) / 2));
    if (clamped_radius <= 0) {
        return true;
    }

    if ((x >= clamped_radius && x < width - clamped_radius) ||
        (y >= clamped_radius && y < height - clamped_radius)) {
        return true;
    }

    const int corner_center_x = x < clamped_radius ? clamped_radius - 1 : width - clamped_radius;
    const int corner_center_y = y < clamped_radius ? clamped_radius - 1 : height - clamped_radius;
    const int dx = x - corner_center_x;
    const int dy = y - corner_center_y;
    const int rr = clamped_radius - 1;
    return (dx * dx) + (dy * dy) <= (rr * rr) + rr;
}

}  // namespace

int ClampPositive(int value)
{
    return std::max(0, value);
}

int CenterOffset(int container_size, int item_size)
{
    return std::max(0, (container_size - item_size) / 2);
}

std::vector<std::string> WrapTextToWidth(design::TypographyRole role,
                                         const std::string& text,
                                         int max_width)
{
    std::vector<std::string> lines;
    if (text.empty()) {
        return lines;
    }
    if (max_width <= 0 || MeasureText(role, text) <= max_width) {
        lines.push_back(text);
        return lines;
    }

    size_t start = 0;
    while (start < text.size()) {
        while (start < text.size() && text[start] == ' ') {
            ++start;
        }
        if (start >= text.size()) {
            break;
        }

        size_t best_end = start;
        size_t pos = start;
        while (pos < text.size()) {
            size_t word_end = text.find(' ', pos);
            if (word_end == std::string::npos) {
                word_end = text.size();
            }
            if (MeasureText(role, text.substr(start, word_end - start)) > max_width) {
                if (best_end == start) {
                    best_end = word_end;  // a single word longer than the line: keep it whole
                }
                break;
            }
            best_end = word_end;
            if (word_end >= text.size()) {
                break;
            }
            pos = word_end + 1;
        }

        lines.push_back(text.substr(start, best_end - start));
        start = best_end + (best_end < text.size() ? 1 : 0);
    }
    return lines;
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
        // 25% coverage, dispersed diagonally rather than as a (x%2==0 && y%2==0) lattice.
        //
        // DrawPortraitPixel maps portrait x onto the panel's gate line (raw_y). Under the old
        // lattice every odd gate line was left completely empty while even ones carried 50%
        // black, so a large fill drove the panel with a gate-periodic pattern that the partial
        // waveform could not settle evenly -- that was the WiFi page banding. It is also why an
        // isolated sparse-dither glyph would not develop until a later refresh (see the
        // carousel::kDisabledControlColor note in design_tokens.h).
        //
        // (x + y) % 4 gives every gate line and every source line exactly 25% coverage, just
        // phase-shifted, so no line is starved and nothing aligns with the drive axes.
        return ((x + y) & 3) == 0;
    }
    return false;
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

void FillRoundedPortraitRect(uint8_t* framebuffer,
                             int raw_width,
                             int raw_height,
                             int portrait_width,
                             int portrait_height,
                             const UiRect& rect,
                             int radius,
                             uint8_t tone)
{
    if (rect.IsEmpty()) {
        return;
    }

    for (int row = 0; row < rect.height; ++row) {
        for (int col = 0; col < rect.width; ++col) {
            if (!RoundedRectContains(rect.width, rect.height, radius, col, row)) {
                continue;
            }
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

void DrawRoundedPortraitBorder(uint8_t* framebuffer,
                               int raw_width,
                               int raw_height,
                               int portrait_width,
                               int portrait_height,
                               const UiRect& rect,
                               int radius,
                               int thickness,
                               uint8_t tone)
{
    if (rect.IsEmpty() || thickness <= 0) {
        return;
    }

    const int border = std::min(thickness, std::min(rect.width, rect.height) / 2);
    if (border <= 0) {
        return;
    }

    for (int row = 0; row < rect.height; ++row) {
        for (int col = 0; col < rect.width; ++col) {
            const bool in_outer = RoundedRectContains(rect.width, rect.height, radius, col, row);
            const bool in_inner =
                RoundedRectContains(rect.width - (2 * border),
                                    rect.height - (2 * border),
                                    std::max(0, radius - border),
                                    col - border,
                                    row - border);
            if (!in_outer || in_inner) {
                continue;
            }

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

            const int px = x + col;
            const int py = y + row;
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

void DrawScaledPortraitMonoAsset(uint8_t* framebuffer,
                                 int raw_width,
                                 int raw_height,
                                 int portrait_width,
                                 int portrait_height,
                                 const UiRect& dest,
                                 const EmbeddedImageAsset* asset,
                                 uint8_t tone)
{
    if (framebuffer == nullptr || asset == nullptr || asset->format != ImageFormat::kMono1 ||
        dest.width <= 0 || dest.height <= 0 || asset->width == 0 || asset->height == 0) {
        return;
    }

    for (int y = 0; y < dest.height; ++y) {
        const int source_y = std::min<int>(
            asset->height - 1, static_cast<int>(static_cast<int64_t>(y) * asset->height / dest.height));
        for (int x = 0; x < dest.width; ++x) {
            const int source_x = std::min<int>(
                asset->width - 1, static_cast<int>(static_cast<int64_t>(x) * asset->width / dest.width));
            if (!AssetPixelSet(*asset, source_x, source_y)) {
                continue;
            }
            const int px = dest.x + x;
            const int py = dest.y + y;
            DrawPortraitPixel(framebuffer, raw_width, raw_height, portrait_width, portrait_height, px,
                              py, ShouldDrawBlackForTone(px, py, tone));
        }
    }
}

void DrawTypographyText(uint8_t* framebuffer,
                        int raw_width,
                        int raw_height,
                        int portrait_width,
                        int portrait_height,
                        int x,
                        int y,
                        std::string_view text,
                        design::TypographyRole role,
                        uint8_t tone)
{
    DrawText(
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
        role);
}

}  // namespace epaper_ui
