#include "display_service.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/lock_screen.h"
#include "epaper_ui/shutdown_modal.h"
#include "epaper_ui/toast.h"
#include "epaper_panel.h"
#include "esp_check.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_assets.h"
#include "shared_bus_service.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace display_service {
namespace {

constexpr const char* kTag = "DisplayService";
constexpr int kPortraitWidth = STICKY_EPD_HEIGHT;
constexpr int kPortraitHeight = STICKY_EPD_WIDTH;
constexpr int kSplashLogoGap = design::spacing::k16;
constexpr int kDemoCardWidth = 360;
constexpr int kDemoCardHeight = 220;
constexpr int kDemoCardX = (kPortraitWidth - kDemoCardWidth) / 2;
constexpr int kDemoCardY = (kPortraitHeight - kDemoCardHeight) / 2;
constexpr int kDemoTextGap = design::spacing::k12;
constexpr uint32_t kDisplayTaskStackWords = 4096;
constexpr auto kDemoTitleRole = design::TypographyRole::kHeadingH2;
constexpr auto kDemoValueRole = design::TypographyRole::kDisplay;

enum class DisplayCommandType {
    kSelectSelection,
    kSetScreen,
    kRefreshCurrent,
};

struct DisplayCommand {
    DisplayCommandType type = DisplayCommandType::kSelectSelection;
    ScreenId screen = ScreenId::kHome;
    DemoSelection selection = DemoSelection::kTop;
    RefreshMode refresh_mode = RefreshMode::kPartial;
};

bool s_initialized = false;
QueueHandle_t s_command_queue = nullptr;
TaskHandle_t s_display_task = nullptr;
std::mutex s_panel_mutex;
bool s_display_sleeping = false;
std::atomic<bool> s_refresh_in_progress = false;
ScreenId s_current_screen = ScreenId::kHome;
DemoSelection s_current_selection = DemoSelection::kTop;
epaper_ui::StatusBarState s_status_bar_state = {};
epaper_ui::GlobalFooterState s_global_footer_state = {};
epaper_ui::LockScreenState s_lock_screen_state = {};
epaper_ui::ShutdownModalState s_shutdown_modal_state = {};
epaper_ui::SelectModalState s_select_modal_state = {};
epaper_ui::ToastState s_toast_state = {};

class RefreshBusyGuard {
public:
    RefreshBusyGuard()
    {
        s_refresh_in_progress.store(true, std::memory_order_relaxed);
    }

    ~RefreshBusyGuard()
    {
        s_refresh_in_progress.store(false, std::memory_order_relaxed);
    }

    RefreshBusyGuard(const RefreshBusyGuard&) = delete;
    RefreshBusyGuard& operator=(const RefreshBusyGuard&) = delete;
};

class DisplayBusGuard {
public:
    explicit DisplayBusGuard(esp_err_t err) : err_(err) {}

    ~DisplayBusGuard()
    {
        if (err_ == ESP_OK) {
            shared_bus_service::ReleaseDisplay();
        }
    }

    esp_err_t err() const { return err_; }

private:
    esp_err_t err_ = ESP_FAIL;
};

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

void DrawPortraitMonoAsset(uint8_t* framebuffer, int x, int y, const EmbeddedImageAsset* asset)
{
    if (framebuffer == nullptr || asset == nullptr || asset->format != ImageFormat::kMono1) {
        return;
    }

    for (int row = 0; row < asset->height; ++row) {
        for (int col = 0; col < asset->width; ++col) {
            if (AssetPixelSet(*asset, col, row)) {
                DrawPortraitPixel(framebuffer, x + col, y + row, true);
            }
        }
    }
}

void DrawTypographyText(uint8_t* framebuffer,
                        int x,
                        int y,
                        std::string_view text,
                        design::TypographyRole role,
                        bool black)
{
    epaper_ui::DrawText(
        [&](int px, int py, uint8_t color) {
            DrawPortraitPixel(framebuffer, px, py, color < 0x80);
        },
        x,
        y,
        text,
        black ? design::color::kBlack : design::color::kWhite,
        role);
}

void DrawActionInRect(uint8_t* framebuffer,
                      int x,
                      int y,
                      int width,
                      int height,
                      std::string_view first,
                      std::string_view second,
                      bool selected)
{
    const int first_height = epaper_ui::LineHeight(kDemoTitleRole);
    const int second_height = epaper_ui::LineHeight(kDemoValueRole);
    const int block_height = first_height + second_height + kDemoTextGap;
    const int first_x = x + (width - epaper_ui::MeasureText(kDemoTitleRole, first)) / 2;
    const int second_x = x + (width - epaper_ui::MeasureText(kDemoValueRole, second)) / 2;
    const int first_y = y + (height - block_height) / 2;
    const int second_y = first_y + first_height + kDemoTextGap;
    const bool text_black = !selected;

    FillPortraitRect(framebuffer, x, y, width, height, selected);
    DrawTypographyText(framebuffer, first_x, first_y, first, kDemoTitleRole, text_black);
    DrawTypographyText(framebuffer, second_x, second_y, second, kDemoValueRole, text_black);
}

void DrawDemoFrame(uint8_t* framebuffer, DemoSelection selection)
{
    DrawActionInRect(framebuffer,
                     kDemoCardX,
                     kDemoCardY,
                     kDemoCardWidth,
                     kDemoCardHeight,
                     "FORMAT",
                     "SD",
                     selection == DemoSelection::kTop);
}

void DrawSplashScreen(uint8_t* framebuffer)
{
    const EmbeddedImageAsset* followup_logo =
        project_assets::GetLogo(EmbeddedLogoId::kFollowupLogo);
    const EmbeddedImageAsset* alxv_logo =
        project_assets::GetLogo(EmbeddedLogoId::kAlxvLabsLogo);
    if (framebuffer == nullptr || followup_logo == nullptr || alxv_logo == nullptr) {
        return;
    }

    const int content_width = std::max<int>(followup_logo->width, alxv_logo->width);
    const int content_height = static_cast<int>(followup_logo->height) + kSplashLogoGap +
                               static_cast<int>(alxv_logo->height);
    const int content_x = (kPortraitWidth - content_width) / 2;
    const int content_y = (kPortraitHeight - content_height) / 2;

    const int followup_x = content_x + (content_width - followup_logo->width) / 2;
    const int followup_y = content_y;
    DrawPortraitMonoAsset(framebuffer, followup_x, followup_y, followup_logo);

    const int alxv_x = content_x + (content_width - alxv_logo->width) / 2;
    const int alxv_y = followup_y + followup_logo->height + kSplashLogoGap;
    DrawPortraitMonoAsset(framebuffer, alxv_x, alxv_y, alxv_logo);
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

const char* RefreshModeName(RefreshMode refresh_mode)
{
    switch (refresh_mode) {
        case RefreshMode::kPartial:
            return "partial";
        case RefreshMode::kFull:
            return "full";
        default:
            return "unknown";
    }
}

esp_err_t ApplyDemoSelection(DemoSelection selection, bool full_refresh)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    DrawDemoFrame(panel.framebuffer(), selection);
    epaper_ui::DrawStatusBar(panel.framebuffer(),
                             STICKY_EPD_WIDTH,
                             STICKY_EPD_HEIGHT,
                             kPortraitWidth,
                             kPortraitHeight,
                             s_status_bar_state);
    epaper_ui::DrawGlobalFooter(panel.framebuffer(),
                                STICKY_EPD_WIDTH,
                                STICKY_EPD_HEIGHT,
                                kPortraitWidth,
                                kPortraitHeight,
                                s_global_footer_state);
    epaper_ui::DrawToast(panel.framebuffer(),
                         STICKY_EPD_WIDTH,
                         STICKY_EPD_HEIGHT,
                         kPortraitWidth,
                         kPortraitHeight,
                         s_toast_state);
    epaper_ui::DrawSelectModal(panel.framebuffer(),
                               STICKY_EPD_WIDTH,
                               STICKY_EPD_HEIGHT,
                               kPortraitWidth,
                               kPortraitHeight,
                               s_select_modal_state);
    epaper_ui::DrawShutdownModal(panel.framebuffer(),
                                 STICKY_EPD_WIDTH,
                                 STICKY_EPD_HEIGHT,
                                 kPortraitWidth,
                                 kPortraitHeight,
                                 s_shutdown_modal_state);

    DisplayBusGuard bus_guard(shared_bus_service::AcquireDisplay());
    if (bus_guard.err() != ESP_OK) {
        return bus_guard.err();
    }

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = full_refresh ? panel.RefreshFullBase()
                                       : panel.RefreshPartialFullScreen();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen = ScreenId::kHome;
    s_current_selection = selection;
    return ESP_OK;
}

esp_err_t ApplyLockScreen(bool full_refresh)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawLockScreen(panel.framebuffer(),
                              STICKY_EPD_WIDTH,
                              STICKY_EPD_HEIGHT,
                              kPortraitWidth,
                              kPortraitHeight,
                              s_lock_screen_state,
                              s_status_bar_state);
    epaper_ui::DrawToast(panel.framebuffer(),
                         STICKY_EPD_WIDTH,
                         STICKY_EPD_HEIGHT,
                         kPortraitWidth,
                         kPortraitHeight,
                         s_toast_state);
    epaper_ui::DrawSelectModal(panel.framebuffer(),
                               STICKY_EPD_WIDTH,
                               STICKY_EPD_HEIGHT,
                               kPortraitWidth,
                               kPortraitHeight,
                               s_select_modal_state);
    epaper_ui::DrawShutdownModal(panel.framebuffer(),
                                 STICKY_EPD_WIDTH,
                                 STICKY_EPD_HEIGHT,
                                 kPortraitWidth,
                                 kPortraitHeight,
                                 s_shutdown_modal_state);

    DisplayBusGuard bus_guard(shared_bus_service::AcquireDisplay());
    if (bus_guard.err() != ESP_OK) {
        return bus_guard.err();
    }

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = full_refresh ? panel.RefreshFullBase()
                                       : panel.RefreshPartialFullScreen();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen = ScreenId::kLockScreen;
    return ESP_OK;
}

esp_err_t ApplyStartupSplash()
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    DrawSplashScreen(panel.framebuffer());

    DisplayBusGuard bus_guard(shared_bus_service::AcquireDisplay());
    if (bus_guard.err() != ESP_OK) {
        return bus_guard.err();
    }

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = panel.RefreshFullBase();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

esp_err_t RefreshCurrentSelectionLocked(bool full_refresh)
{
    switch (s_current_screen) {
        case ScreenId::kLockScreen:
            return ApplyLockScreen(full_refresh);
        case ScreenId::kHome:
        default:
            return ApplyDemoSelection(s_current_selection, full_refresh);
    }
}

esp_err_t SleepPanelLocked(const char* reason)
{
    DisplayBusGuard bus_guard(shared_bus_service::AcquireDisplay());
    if (bus_guard.err() != ESP_OK) {
        return bus_guard.err();
    }

    EpaperPanel& panel = Panel();
    ESP_RETURN_ON_ERROR(panel.Sleep(), kTag, "panel sleep failed");
    s_display_sleeping = true;
    ESP_LOGI(kTag, "Panel entered sleep for %s", reason);
    return ESP_OK;
}

void DisplayTask(void*)
{
    DisplayCommand command = {};
    command.type = DisplayCommandType::kSelectSelection;
    command.selection = DemoSelection::kTop;
    command.refresh_mode = RefreshMode::kPartial;
    while (true) {
        if (xQueueReceive(s_command_queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(kTag, "Display command requested: type=%d screen=%d mode=%s",
                 static_cast<int>(command.type),
                 static_cast<int>(command.screen),
                 RefreshModeName(command.refresh_mode));
        std::lock_guard<std::mutex> lock(s_panel_mutex);
        if (s_display_sleeping) {
            if (command.type == DisplayCommandType::kSelectSelection) {
                s_current_screen = ScreenId::kHome;
                s_current_selection = command.selection;
            } else if (command.type == DisplayCommandType::kSetScreen) {
                s_current_screen = command.screen;
            }
            ESP_LOGI(kTag, "Display command suppressed while display sleeping");
            continue;
        }

        esp_err_t err = ESP_OK;
        if (command.type == DisplayCommandType::kSelectSelection) {
            err = ApplyDemoSelection(command.selection,
                                     command.refresh_mode == RefreshMode::kFull);
        } else if (command.type == DisplayCommandType::kSetScreen) {
            err = command.screen == ScreenId::kLockScreen
                      ? ApplyLockScreen(command.refresh_mode == RefreshMode::kFull)
                      : ApplyDemoSelection(s_current_selection,
                                           command.refresh_mode == RefreshMode::kFull);
        } else {
            err = RefreshCurrentSelectionLocked(command.refresh_mode == RefreshMode::kFull);
        }
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Display refresh failed (mode=%s): %s",
                     RefreshModeName(command.refresh_mode), esp_err_to_name(err));
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

    const BaseType_t created = xTaskCreatePinnedToCore(
        DisplayTask,
        "display_service",
        kDisplayTaskStackWords,
        nullptr,
        followup_task_config::kPriorityDisplay,
        &s_display_task,
        followup_task_config::kAppCore);
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

    ESP_RETURN_ON_ERROR(shared_bus_service::Init(), kTag, "shared bus init failed");
    ESP_RETURN_ON_ERROR(sticky_board::EnsureSharedSpiBus(), kTag, "shared SPI bus init failed");
    ESP_RETURN_ON_ERROR(sticky_board::EnableEpaperPower(), kTag, "enable e-paper power failed");

    DisplayBusGuard bus_guard(shared_bus_service::AcquireDisplay());
    ESP_RETURN_ON_ERROR(bus_guard.err(), kTag, "shared display bus acquire failed");

    EpaperPanel& panel = Panel();
    ESP_RETURN_ON_ERROR(panel.Initialize(), kTag, "panel initialize failed");
    {
        std::lock_guard<std::mutex> lock(s_panel_mutex);
        ESP_RETURN_ON_ERROR(ApplyStartupSplash(),
                            kTag,
                            "panel startup splash refresh failed");
    }
    ESP_RETURN_ON_ERROR(StartDisplayTask(), kTag, "display task init failed");

    s_initialized = true;
    ESP_LOGI(kTag, "Display initialized with startup splash");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

int PortraitWidth()
{
    return kPortraitWidth;
}

int PortraitHeight()
{
    return kPortraitHeight;
}

ScreenId GetCurrentScreen()
{
    std::lock_guard<std::mutex> lock(s_panel_mutex);
    return s_current_screen;
}

esp_err_t SetStatusBarState(const epaper_ui::StatusBarState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_status_bar_state = state;
    return ESP_OK;
}

esp_err_t SetGlobalFooterState(const epaper_ui::GlobalFooterState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_global_footer_state = state;
    return ESP_OK;
}

esp_err_t SetLockScreenState(const epaper_ui::LockScreenState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_lock_screen_state = state;
    return ESP_OK;
}

esp_err_t SetShutdownModalState(const epaper_ui::ShutdownModalState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_shutdown_modal_state = state;
    return ESP_OK;
}

esp_err_t SetSelectModalState(const epaper_ui::SelectModalState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_select_modal_state = state;
    return ESP_OK;
}

esp_err_t SetToastState(const epaper_ui::ToastState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    s_toast_state = state;
    return ESP_OK;
}

esp_err_t SetCurrentScreen(ScreenId screen, RefreshMode refresh_mode)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kSetScreen;
    command.screen = screen;
    command.selection = s_current_selection;
    command.refresh_mode = refresh_mode;
    return xQueueOverwrite(s_command_queue, &command) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t SelectDemoSelection(DemoSelection selection, RefreshMode refresh_mode)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kSelectSelection;
    command.screen = ScreenId::kHome;
    command.selection = selection;
    command.refresh_mode = refresh_mode;
    return xQueueOverwrite(s_command_queue, &command) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t RequestRefreshCurrentScreen(RefreshMode refresh_mode)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kRefreshCurrent;
    command.screen = s_current_screen;
    command.selection = s_current_selection;
    command.refresh_mode = refresh_mode;
    return xQueueOverwrite(s_command_queue, &command) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t RefreshCurrentScreen(RefreshMode refresh_mode)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    if (s_display_sleeping) {
        return ESP_OK;
    }

    return RefreshCurrentSelectionLocked(refresh_mode == RefreshMode::kFull);
}

esp_err_t EnterDisplaySleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    if (s_display_sleeping) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(SleepPanelLocked("display sleep"), kTag, "display sleep failed");
    ESP_LOGI(kTag, "Display entered sleep");
    return ESP_OK;
}

esp_err_t EnterLightSleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    if (s_display_sleeping) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(SleepPanelLocked("light sleep"), kTag, "light sleep failed");
    ESP_LOGI(kTag, "Display prepared for light sleep");
    return ESP_OK;
}

esp_err_t WakeDisplay()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    if (!s_display_sleeping) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(RefreshCurrentSelectionLocked(true),
                        kTag,
                        "display wake refresh failed");
    s_display_sleeping = false;
    ESP_LOGI(kTag, "Display woke with full refresh");
    return ESP_OK;
}

esp_err_t RecoverAfterLightSleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    ESP_RETURN_ON_ERROR(RefreshCurrentSelectionLocked(true),
                        kTag,
                        "display light-sleep recovery refresh failed");
    s_display_sleeping = false;
    ESP_LOGI(kTag, "Display recovered after light sleep with forced full refresh");
    return ESP_OK;
}

bool IsRefreshInProgress()
{
    return s_refresh_in_progress.load(std::memory_order_relaxed);
}

}  // namespace display_service
