#include "display_service.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "epaper_panel.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace display_service {
namespace {

constexpr const char* kTag = "DisplayService";
constexpr int kPortraitWidth = STICKY_EPD_HEIGHT;
constexpr int kPortraitHeight = STICKY_EPD_WIDTH;
constexpr int kTextScale = 8;
constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kGlyphAdvance = 6;
constexpr int kLineGap = 16;

bool s_initialized = false;

EpaperPanelConfig BuildPanelConfig()
{
    EpaperPanelConfig config = {};
    config.spi_host = STICKY_SHARED_SPI_HOST;
    config.cs = STICKY_EPD_CS_PIN;
    config.dc = STICKY_EPD_DC_PIN;
    config.rst = STICKY_EPD_RST_PIN;
    config.busy = STICKY_EPD_BUSY_PIN;
    config.mosi = STICKY_SHARED_SPI_MOSI_PIN;
    config.miso = STICKY_SHARED_SPI_MISO_PIN;
    config.sck = STICKY_SHARED_SPI_CLK_PIN;
    config.external_spi_bus = true;
    config.buffer_len = STICKY_EPD_BUFFER_LEN;
    config.busy_timeout_ms = 10000;
    config.reset_low_ms = 2;
    config.reset_high_ms = 50;
    config.busy_level = 1;
    return config;
}

EpaperPanel& Panel()
{
    static EpaperPanel panel(STICKY_EPD_WIDTH, STICKY_EPD_HEIGHT, BuildPanelConfig());
    return panel;
}

const uint8_t* GlyphFor(char c)
{
    static constexpr uint8_t kSpace[kGlyphHeight] = {
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
        0b00000,
    };
    static constexpr uint8_t kH[kGlyphHeight] = {
        0b10001,
        0b10001,
        0b10001,
        0b11111,
        0b10001,
        0b10001,
        0b10001,
    };
    static constexpr uint8_t kD[kGlyphHeight] = {
        0b11110,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b11110,
    };
    static constexpr uint8_t kE[kGlyphHeight] = {
        0b11111,
        0b10000,
        0b10000,
        0b11110,
        0b10000,
        0b10000,
        0b11111,
    };
    static constexpr uint8_t kL[kGlyphHeight] = {
        0b10000,
        0b10000,
        0b10000,
        0b10000,
        0b10000,
        0b10000,
        0b11111,
    };
    static constexpr uint8_t kO[kGlyphHeight] = {
        0b01110,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b10001,
        0b01110,
    };
    static constexpr uint8_t kR[kGlyphHeight] = {
        0b11110,
        0b10001,
        0b10001,
        0b11110,
        0b10100,
        0b10010,
        0b10001,
    };
    static constexpr uint8_t kW[kGlyphHeight] = {
        0b10001,
        0b10001,
        0b10001,
        0b10101,
        0b10101,
        0b10101,
        0b01010,
    };

    switch (c) {
        case 'H':
        case 'h':
            return kH;
        case 'D':
        case 'd':
            return kD;
        case 'E':
        case 'e':
            return kE;
        case 'L':
        case 'l':
            return kL;
        case 'O':
        case 'o':
            return kO;
        case 'R':
        case 'r':
            return kR;
        case 'W':
        case 'w':
            return kW;
        default:
            return kSpace;
    }
}

int TextWidth(std::string_view text)
{
    if (text.empty()) {
        return 0;
    }
    return static_cast<int>((text.size() - 1) * kGlyphAdvance + kGlyphWidth) * kTextScale;
}

void DrawRawPixel(uint8_t* framebuffer, int x, int y, bool black)
{
    if (framebuffer == nullptr || x < 0 || y < 0 || x >= STICKY_EPD_WIDTH ||
        y >= STICKY_EPD_HEIGHT) {
        return;
    }

    const size_t index = static_cast<size_t>(y) * (STICKY_EPD_WIDTH / 8) +
                         static_cast<size_t>(x / 8);
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x07));
    if (black) {
        framebuffer[index] &= static_cast<uint8_t>(~mask);
    } else {
        framebuffer[index] |= mask;
    }
}

void DrawPortraitPixel(uint8_t* framebuffer, int x, int y, bool black)
{
    if (x < 0 || y < 0 || x >= kPortraitWidth || y >= kPortraitHeight) {
        return;
    }

    const int raw_x = y;
    const int raw_y = STICKY_EPD_HEIGHT - 1 - x;
    DrawRawPixel(framebuffer, raw_x, raw_y, black);
}

void FillPortraitRect(uint8_t* framebuffer, int x, int y, int width, int height, bool black)
{
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            DrawPortraitPixel(framebuffer, x + col, y + row, black);
        }
    }
}

void DrawText(uint8_t* framebuffer, int x, int y, std::string_view text)
{
    int cursor_x = x;
    for (const char c : text) {
        const uint8_t* glyph = GlyphFor(c);
        for (int row = 0; row < kGlyphHeight; ++row) {
            for (int col = 0; col < kGlyphWidth; ++col) {
                const bool on = (glyph[row] & (1U << (kGlyphWidth - 1 - col))) != 0;
                if (on) {
                    FillPortraitRect(framebuffer,
                                     cursor_x + col * kTextScale,
                                     y + row * kTextScale,
                                     kTextScale,
                                     kTextScale,
                                     true);
                }
            }
        }
        cursor_x += kGlyphAdvance * kTextScale;
    }
}

void DrawHelloWorld(uint8_t* framebuffer)
{
    constexpr std::string_view kHello = "Hello";
    constexpr std::string_view kWorld = "world";
    const int text_height = kGlyphHeight * kTextScale;
    const int block_height = text_height * 2 + kLineGap;
    const int hello_x = (kPortraitWidth - TextWidth(kHello)) / 2;
    const int world_x = (kPortraitWidth - TextWidth(kWorld)) / 2;
    const int hello_y = (kPortraitHeight - block_height) / 2;
    const int world_y = hello_y + text_height + kLineGap;

    DrawText(framebuffer, hello_x, hello_y, kHello);
    DrawText(framebuffer, world_x, world_y, kWorld);
}

void LogMetrics(const EpaperPanelMetrics& metrics)
{
    ESP_LOGI(kTag,
             "refresh metrics: busy=%lldus spi=%lldus reset=%lldus trigger=%lldus init=%lldus",
             static_cast<long long>(metrics.panel_busy_us),
             static_cast<long long>(metrics.spi_transfer_us),
             static_cast<long long>(metrics.reset_sequence_us),
             static_cast<long long>(metrics.trigger_us),
             static_cast<long long>(metrics.init_ready_us));
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(sticky_board::EnsureSharedSpiBus(), kTag, "shared SPI bus init failed");
    ESP_RETURN_ON_ERROR(sticky_board::EnableEpaperPower(), kTag, "enable e-paper power failed");

    EpaperPanel& panel = Panel();
    ESP_RETURN_ON_ERROR(panel.Initialize(), kTag, "panel initialize failed");
    panel.Clear(true);
    DrawHelloWorld(panel.framebuffer());
    ESP_RETURN_ON_ERROR(panel.RefreshFullBase(), kTag, "panel base refresh failed");
    LogMetrics(panel.metrics());

    s_initialized = true;
    ESP_LOGI(kTag, "Display initialized with portrait Hello world screen");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

}  // namespace display_service
