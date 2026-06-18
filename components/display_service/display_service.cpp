#include "display_service.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "epaper_panel.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace display_service {
namespace {

constexpr const char* kTag = "DisplayService";
constexpr int kPortraitWidth = STICKY_EPD_HEIGHT;
constexpr int kPortraitHeight = STICKY_EPD_WIDTH;
constexpr int kTextScale = 5;
constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kGlyphAdvance = 6;
constexpr int kLineGap = 12;
constexpr int kDemoCardWidth = 360;
constexpr int kDemoCardHeight = 220;
constexpr int kDemoCardGap = 64;
constexpr int kDemoCardX = (kPortraitWidth - kDemoCardWidth) / 2;
constexpr int kDemoCardsHeight = kDemoCardHeight * 2 + kDemoCardGap;
constexpr int kDemoCardTopY = (kPortraitHeight - kDemoCardsHeight) / 2;
constexpr int kDemoCardBottomY = kDemoCardTopY + kDemoCardHeight + kDemoCardGap;
constexpr uint32_t kDisplayTaskStackWords = 4096;
constexpr UBaseType_t kDisplayTaskPriority = 4;

struct DisplayCommand {
    DemoSelection selection;
};

bool s_initialized = false;
QueueHandle_t s_command_queue = nullptr;
TaskHandle_t s_display_task = nullptr;

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

void DrawText(uint8_t* framebuffer, int x, int y, std::string_view text, bool black)
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
                                     black);
                }
            }
        }
        cursor_x += kGlyphAdvance * kTextScale;
    }
}

void DrawHelloWorldInRect(uint8_t* framebuffer,
                          int x,
                          int y,
                          int width,
                          int height,
                          bool selected)
{
    constexpr std::string_view kHello = "Hello";
    constexpr std::string_view kWorld = "world";
    const int text_height = kGlyphHeight * kTextScale;
    const int block_height = text_height * 2 + kLineGap;
    const int hello_x = x + (width - TextWidth(kHello)) / 2;
    const int world_x = x + (width - TextWidth(kWorld)) / 2;
    const int hello_y = y + (height - block_height) / 2;
    const int world_y = hello_y + text_height + kLineGap;
    const bool text_black = !selected;

    FillPortraitRect(framebuffer, x, y, width, height, selected);
    DrawText(framebuffer, hello_x, hello_y, kHello, text_black);
    DrawText(framebuffer, world_x, world_y, kWorld, text_black);
}

void DrawDemoFrame(uint8_t* framebuffer, DemoSelection selection)
{
    DrawHelloWorldInRect(framebuffer,
                         kDemoCardX,
                         kDemoCardTopY,
                         kDemoCardWidth,
                         kDemoCardHeight,
                         selection == DemoSelection::kTop);
    DrawHelloWorldInRect(framebuffer,
                         kDemoCardX,
                         kDemoCardBottomY,
                         kDemoCardWidth,
                         kDemoCardHeight,
                         selection == DemoSelection::kBottom);
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

esp_err_t ApplyDemoSelection(DemoSelection selection, bool full_refresh)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    DrawDemoFrame(panel.framebuffer(), selection);

    const esp_err_t err = full_refresh ? panel.RefreshFullBase()
                                       : panel.RefreshPartialFullScreen();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

void DisplayTask(void*)
{
    DisplayCommand command{DemoSelection::kTop};
    while (true) {
        if (xQueueReceive(s_command_queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(kTag, "Demo selection requested: %s",
                 command.selection == DemoSelection::kTop ? "top" : "bottom");
        const esp_err_t err = ApplyDemoSelection(command.selection, false);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Demo partial refresh failed: %s", esp_err_to_name(err));
        }
    }
}

esp_err_t StartDisplayTask()
{
    if (s_command_queue == nullptr) {
        s_command_queue = xQueueCreate(1, sizeof(DisplayCommand));
        if (s_command_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_display_task != nullptr) {
        return ESP_OK;
    }

    const BaseType_t created = xTaskCreate(DisplayTask,
                                          "display_service",
                                          kDisplayTaskStackWords,
                                          nullptr,
                                          kDisplayTaskPriority,
                                          &s_display_task);
    if (created != pdPASS) {
        s_display_task = nullptr;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
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
    ESP_RETURN_ON_ERROR(ApplyDemoSelection(DemoSelection::kTop, true),
                        kTag,
                        "panel base refresh failed");
    ESP_RETURN_ON_ERROR(StartDisplayTask(), kTag, "display task init failed");

    s_initialized = true;
    ESP_LOGI(kTag, "Display initialized with portrait partial refresh demo");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

esp_err_t SelectDemoSelection(DemoSelection selection)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const DisplayCommand command{selection};
    return xQueueOverwrite(s_command_queue, &command) == pdPASS ? ESP_OK : ESP_FAIL;
}

}  // namespace display_service
