#include "epaper_ui/status_bar.h"

#include <algorithm>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/font_renderer.h"
#include "project_assets.h"

namespace epaper_ui {
namespace {

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
                      int x,
                      int y,
                      int width,
                      int height,
                      uint8_t tone)
{
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
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

template <typename DrawFn>
void DrawOutlined(DrawFn&& draw_fn, int stroke_thickness)
{
    const int thickness = std::max(0, stroke_thickness);
    for (int dy = -thickness; dy <= thickness; ++dy) {
        for (int dx = -thickness; dx <= thickness; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            draw_fn(dx, dy, design::color::kWhite);
        }
    }
    draw_fn(0, 0, design::color::kBlack);
}

void DrawOutlinedText(uint8_t* framebuffer,
                      int raw_width,
                      int raw_height,
                      int portrait_width,
                      int portrait_height,
                      int x,
                      int y,
                      std::string_view text,
                      design::TypographyRole role,
                      int stroke_thickness)
{
    DrawOutlined(
        [&](int dx, int dy, uint8_t tone) {
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
                x + dx,
                y + dy,
                text,
                tone,
                role);
        },
        stroke_thickness);
}

void DrawOutlinedAsset(uint8_t* framebuffer,
                       int raw_width,
                       int raw_height,
                       int portrait_width,
                       int portrait_height,
                       int x,
                       int y,
                       const EmbeddedImageAsset* asset,
                       int stroke_thickness)
{
    if (asset == nullptr) {
        return;
    }

    DrawOutlined(
        [&](int dx, int dy, uint8_t tone) {
            DrawPortraitMonoAsset(framebuffer,
                                  raw_width,
                                  raw_height,
                                  portrait_width,
                                  portrait_height,
                                  x + dx,
                                  y + dy,
                                  asset,
                                  tone);
        },
        stroke_thickness);
}

const EmbeddedImageAsset* ResolveBatteryIcon(const BatteryStatus& battery)
{
    if (battery.charging) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery6);
    }
    if (battery.percent < 0 || battery.percent <= 10) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery1);
    }
    if (battery.percent <= 30) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery2);
    }
    if (battery.percent <= 50) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery3);
    }
    if (battery.percent <= 75) {
        return project_assets::GetIcon(EmbeddedIconId::kBattery4);
    }
    return project_assets::GetIcon(EmbeddedIconId::kBattery5);
}

const EmbeddedImageAsset* ResolveWifiIcon(WifiStatus status)
{
    switch (status) {
        case WifiStatus::kDisconnected:
            return project_assets::GetIcon(EmbeddedIconId::kWifi3);
        case WifiStatus::kConnected:
            return project_assets::GetIcon(EmbeddedIconId::kWifi2);
        case WifiStatus::kAccessPoint:
            return project_assets::GetIcon(EmbeddedIconId::kWifi4);
        case WifiStatus::kDisabled:
        default:
            return project_assets::GetIcon(EmbeddedIconId::kWifi1);
    }
}

int CenterY(int container_top, int container_height, int item_height)
{
    return container_top + std::max(0, (container_height - item_height) / 2);
}

void DrawIconSlot(uint8_t* framebuffer,
                  int raw_width,
                  int raw_height,
                  int portrait_width,
                  int portrait_height,
                  int slot_x,
                  int top,
                  int box_size,
                  const EmbeddedImageAsset* asset,
                  int stroke_thickness)
{
    if (asset == nullptr) {
        return;
    }

    const int draw_x = slot_x + std::max(0, (box_size - static_cast<int>(asset->width)) / 2);
    const int draw_y = top + std::max(0, (box_size - static_cast<int>(asset->height)) / 2);
    DrawOutlinedAsset(framebuffer,
                      raw_width,
                      raw_height,
                      portrait_width,
                      portrait_height,
                      draw_x,
                      draw_y,
                      asset,
                      stroke_thickness);
}

}  // namespace

int StatusBarHeight()
{
    return design::status_bar::kHeight;
}

void DrawStatusBar(uint8_t* framebuffer,
                   int raw_width,
                   int raw_height,
                   int portrait_width,
                   int portrait_height,
                   const StatusBarState& state)
{
    if (framebuffer == nullptr) {
        return;
    }

    const int bar_height = StatusBarHeight();
    FillPortraitRect(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     0,
                     0,
                     portrait_width,
                     bar_height,
                     design::status_bar::kBackgroundColor);

    const int content_x = design::status_bar::kSidePadding;
    const int content_right = portrait_width - design::status_bar::kSidePadding;
    const int icon_box = design::status_bar::kPreferredIconSize;
    const int icon_top = CenterY(0, bar_height, icon_box);
    const auto label_role = design::TypographyRole::kStatusBarLabel;
    const int text_height = LineHeight(label_role);
    const int text_y = CenterY(0, bar_height, text_height);
    const int stroke_thickness = design::status_bar::kStrokeThickness;

    std::string battery_text;
    if (state.battery.percent >= 0) {
        battery_text = std::to_string(std::clamp(state.battery.percent, 0, 100)) + "%";
    }
    const int battery_text_width = MeasureText(label_role, battery_text);

    const EmbeddedImageAsset* battery_icon = ResolveBatteryIcon(state.battery);
    const EmbeddedImageAsset* wifi_icon = ResolveWifiIcon(state.wifi);
    const EmbeddedImageAsset* sleep_icon =
        state.show_sleep_icon ? project_assets::GetIcon(EmbeddedIconId::kSleep) : nullptr;
    const EmbeddedImageAsset* power_icon =
        state.show_power_icon ? project_assets::GetIcon(EmbeddedIconId::kPower) : nullptr;

    int cursor_right = content_right;
    if (battery_text_width > 0) {
        cursor_right -= battery_text_width;
        DrawOutlinedText(framebuffer,
                         raw_width,
                         raw_height,
                         portrait_width,
                         portrait_height,
                         cursor_right,
                         text_y,
                         battery_text,
                         label_role,
                         stroke_thickness);
    }

    cursor_right -= design::status_bar::kItemGap + icon_box;
    DrawIconSlot(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 cursor_right,
                 icon_top,
                 icon_box,
                 battery_icon,
                 stroke_thickness);

    cursor_right -= design::status_bar::kItemGap + icon_box;
    DrawIconSlot(framebuffer,
                 raw_width,
                 raw_height,
                 portrait_width,
                 portrait_height,
                 cursor_right,
                 icon_top,
                 icon_box,
                 wifi_icon,
                 stroke_thickness);

    if (state.show_sleep_icon) {
        cursor_right -= design::status_bar::kItemGap + icon_box;
        DrawIconSlot(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     cursor_right,
                     icon_top,
                     icon_box,
                     sleep_icon,
                     stroke_thickness);
    }

    if (state.show_power_icon) {
        cursor_right -= design::status_bar::kItemGap + icon_box;
        DrawIconSlot(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     cursor_right,
                     icon_top,
                     icon_box,
                     power_icon,
                     stroke_thickness);
    }

    const std::string_view time_text =
        state.time_text.empty() ? std::string_view("--:--") : std::string_view(state.time_text);
    DrawOutlinedText(framebuffer,
                     raw_width,
                     raw_height,
                     portrait_width,
                     portrait_height,
                     content_x,
                     text_y,
                     time_text,
                     label_role,
                     stroke_thickness);
}

}  // namespace epaper_ui
