#include "display_service.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string_view>

#include "design_tokens.h"
#include "epaper_ui/font_renderer.h"
#include "epaper_ui/keyboard.h"
#include "epaper_ui/lock_screen.h"
#include "epaper_ui/settings_page.h"
#include "epaper_ui/card_modal.h"
#include "epaper_ui/sticky_note.h"
#include "epaper_ui/toast.h"
#include "epaper_ui/dashboard_page.h"
#include "epaper_ui/details_page.h"
#include "epaper_ui/follow_up_page.h"
#include "epaper_ui/notes_page.h"
#include "epaper_ui/onboarding_page.h"
#include "epaper_ui/summarize_page.h"
#include "epaper_ui/todos_page.h"
#include "epaper_ui/time_page.h"
#include "epaper_ui/vibe_check_page.h"
#include "epaper_ui/wifi_page.h"
#include "epaper_panel.h"
#include "esp_check.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_assets.h"
#include "waveshare_board_config.h"

namespace display_service {
namespace {

constexpr const char* kTag = "DisplayService";
constexpr int kPortraitWidth = WAVESHARE_EPD_HEIGHT;
constexpr int kPortraitHeight = WAVESHARE_EPD_WIDTH;
constexpr int kSplashLogoGap = design::spacing::k16;
constexpr uint32_t kDisplayTaskStackWords = 4096;

enum class DisplayCommandType {
    kSetScreen,
    kRefreshOverlay,
    kRefreshCurrent,
};

// Caller tag carried with each queued command so the "Display command requested" log
// names what invoked the refresh. A fixed buffer (not a const char*) so it copies by
// value through the FreeRTOS queue and stays valid when the display task logs it on
// another thread.
constexpr size_t kCommandSourceLen = 48;

struct DisplayCommand {
    DisplayCommandType type = DisplayCommandType::kSetScreen;
    ScreenId screen = ScreenId::kHome;
    RefreshRequest refresh_request = {};
    OverlayRefreshPolicy overlay_refresh_policy = OverlayRefreshPolicy::kRebuildUnderlay;
    char source[kCommandSourceLen] = "?";
};

void SetCommandSource(DisplayCommand& command, const char* source)
{
    strlcpy(command.source, source != nullptr ? source : "?", sizeof(command.source));
}

bool s_initialized = false;
QueueHandle_t s_command_queue = nullptr;
TaskHandle_t s_display_task = nullptr;
std::mutex s_state_mutex;
std::mutex s_panel_mutex;
std::mutex s_command_queue_mutex;
bool s_display_sleeping = false;
std::atomic<bool> s_refresh_in_progress = false;
std::atomic<ScreenId> s_current_screen = ScreenId::kHome;
epaper_ui::StatusBarState s_status_bar_state = {};
epaper_ui::GlobalFooterState s_global_footer_state = {};
epaper_ui::SettingsPageState s_settings_page_state = {};
epaper_ui::WifiPageState s_wifi_page_state = {};
epaper_ui::TimePageState s_time_page_state = {};
epaper_ui::DashboardPageState s_dashboard_page_state = {};
epaper_ui::VibeCheckPageState s_vibe_check_page_state = {};
epaper_ui::SummarizePageState s_summarize_page_state = {};
epaper_ui::NotesPageState s_notes_page_state = {};
epaper_ui::TodosPageState s_todos_page_state = {};
epaper_ui::FollowUpPageState s_follow_up_page_state = {};
epaper_ui::DetailsPageState s_details_page_state = {};
epaper_ui::OnboardingPageState s_onboarding_page_state = {};
epaper_ui::LockScreenState s_lock_screen_state = {};
epaper_ui::KeyboardState s_keyboard_state = {};
epaper_ui::CardModalState s_card_modal_state = {};
epaper_ui::SelectModalState s_select_modal_state = {};
epaper_ui::ToastState s_toast_state = {};
epaper_ui::StickyNoteState s_sticky_note_state = {};
std::array<uint8_t, WAVESHARE_EPD_BUFFER_LEN> s_underlay_snapshot = {};
bool s_underlay_snapshot_valid = false;

struct RenderSnapshot {
    epaper_ui::StatusBarState status_bar = {};
    epaper_ui::GlobalFooterState global_footer = {};
    epaper_ui::SettingsPageState settings_page = {};
    epaper_ui::WifiPageState wifi_page = {};
    epaper_ui::TimePageState time_page = {};
    epaper_ui::DashboardPageState dashboard_page = {};
    epaper_ui::VibeCheckPageState vibe_check_page = {};
    epaper_ui::SummarizePageState summarize_page = {};
    epaper_ui::NotesPageState notes_page = {};
    epaper_ui::TodosPageState todos_page = {};
    epaper_ui::FollowUpPageState follow_up_page = {};
    epaper_ui::DetailsPageState details_page = {};
    epaper_ui::OnboardingPageState onboarding_page = {};
    epaper_ui::LockScreenState lock_screen = {};
    epaper_ui::KeyboardState keyboard = {};
    epaper_ui::CardModalState card_modal = {};
    epaper_ui::SelectModalState select_modal = {};
    epaper_ui::ToastState toast = {};
    epaper_ui::StickyNoteState sticky_note = {};
};

// Single shared snapshot reused across every render. The struct is large (it holds every
// page + overlay state), so copying it onto the 4 KB display task stack on each render
// overflowed the stack. Only the display task captures/reads it, and captures never nest,
// so one instance returned by reference is safe and avoids the per-render stack copy.
RenderSnapshot s_render_snapshot = {};

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

EpaperPanelConfig BuildPanelConfig()
{
    EpaperPanelConfig config = {};
    // Waveshare: the EPD owns a dedicated SPI3 bus, so the panel initializes and
    // manages the bus itself (no shared-bus coordination, write-only, no MISO).
    config.spi_host = WAVESHARE_EPD_SPI_HOST;
    config.cs = WAVESHARE_EPD_CS_PIN;
    config.dc = WAVESHARE_EPD_DC_PIN;
    config.rst = WAVESHARE_EPD_RST_PIN;
    config.busy = WAVESHARE_EPD_BUSY_PIN;
    config.mosi = WAVESHARE_EPD_MOSI_PIN;
    config.miso = WAVESHARE_EPD_MISO_PIN;
    config.sck = WAVESHARE_EPD_SCK_PIN;
    config.external_spi_bus = false;
    config.buffer_len = WAVESHARE_EPD_BUFFER_LEN;
    config.busy_timeout_ms = 10000;
    config.reset_low_ms = 2;
    config.reset_high_ms = 50;
    config.busy_level = 1;
    return config;
}

EpaperPanel& Panel()
{
    static EpaperPanel panel(WAVESHARE_EPD_WIDTH, WAVESHARE_EPD_HEIGHT, BuildPanelConfig());
    return panel;
}

const RenderSnapshot& CaptureRenderSnapshot()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    RenderSnapshot& snapshot = s_render_snapshot;
    snapshot.status_bar = s_status_bar_state;
    snapshot.global_footer = s_global_footer_state;
    snapshot.settings_page = s_settings_page_state;
    snapshot.wifi_page = s_wifi_page_state;
    snapshot.time_page = s_time_page_state;
    snapshot.dashboard_page = s_dashboard_page_state;
    snapshot.vibe_check_page = s_vibe_check_page_state;
    snapshot.summarize_page = s_summarize_page_state;
    snapshot.notes_page = s_notes_page_state;
    snapshot.todos_page = s_todos_page_state;
    snapshot.follow_up_page = s_follow_up_page_state;
    snapshot.details_page = s_details_page_state;
    snapshot.onboarding_page = s_onboarding_page_state;
    snapshot.lock_screen = s_lock_screen_state;
    snapshot.keyboard = s_keyboard_state;
    snapshot.card_modal = s_card_modal_state;
    snapshot.select_modal = s_select_modal_state;
    snapshot.toast = s_toast_state;
    snapshot.sticky_note = s_sticky_note_state;
    return snapshot;
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
    if (framebuffer == nullptr || x < 0 || y < 0 || x >= WAVESHARE_EPD_WIDTH ||
        y >= WAVESHARE_EPD_HEIGHT) {
        return;
    }

    const size_t index = static_cast<size_t>(y) * (WAVESHARE_EPD_WIDTH / 8) +
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
    const int raw_y = WAVESHARE_EPD_HEIGHT - 1 - x;
    DrawRawPixel(framebuffer, raw_x, raw_y, black);
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

void DrawCurrentOverlays(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    epaper_ui::DrawKeyboard(framebuffer,
                            WAVESHARE_EPD_WIDTH,
                            WAVESHARE_EPD_HEIGHT,
                            kPortraitWidth,
                            kPortraitHeight,
                            snapshot.keyboard,
                            {});
    epaper_ui::DrawToast(framebuffer,
                         WAVESHARE_EPD_WIDTH,
                         WAVESHARE_EPD_HEIGHT,
                         kPortraitWidth,
                         kPortraitHeight,
                         snapshot.toast);
    epaper_ui::DrawSelectModal(framebuffer,
                               WAVESHARE_EPD_WIDTH,
                               WAVESHARE_EPD_HEIGHT,
                               kPortraitWidth,
                               kPortraitHeight,
                               snapshot.select_modal);
    epaper_ui::DrawCardModal(framebuffer,
                             WAVESHARE_EPD_WIDTH,
                             WAVESHARE_EPD_HEIGHT,
                             kPortraitWidth,
                             kPortraitHeight,
                             snapshot.card_modal);
    // Sticky-note overlay is full-page; draw it on top of every other overlay.
    epaper_ui::DrawStickyNote(framebuffer,
                              WAVESHARE_EPD_WIDTH,
                              WAVESHARE_EPD_HEIGHT,
                              kPortraitWidth,
                              kPortraitHeight,
                              snapshot.sticky_note,
                              {});
}

void CaptureUnderlaySnapshot(const uint8_t* framebuffer)
{
    if (framebuffer == nullptr) {
        s_underlay_snapshot_valid = false;
        return;
    }

    std::memcpy(s_underlay_snapshot.data(), framebuffer, s_underlay_snapshot.size());
    s_underlay_snapshot_valid = true;
}

void DrawHomeUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawDashboardPage(framebuffer,
                                 WAVESHARE_EPD_WIDTH,
                                 WAVESHARE_EPD_HEIGHT,
                                 kPortraitWidth,
                                 kPortraitHeight,
                                 snapshot.dashboard_page,
                                 snapshot.status_bar,
                                 snapshot.global_footer);
}

void DrawLockScreenUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawLockScreen(framebuffer,
                              WAVESHARE_EPD_WIDTH,
                              WAVESHARE_EPD_HEIGHT,
                              kPortraitWidth,
                              kPortraitHeight,
                              snapshot.lock_screen,
                              snapshot.status_bar);
}

void DrawSettingsUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawSettingsPage(framebuffer,
                                WAVESHARE_EPD_WIDTH,
                                WAVESHARE_EPD_HEIGHT,
                                kPortraitWidth,
                                kPortraitHeight,
                                snapshot.settings_page,
                                snapshot.status_bar,
                                snapshot.global_footer);
}

void DrawWifiUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawWifiPage(framebuffer,
                            WAVESHARE_EPD_WIDTH,
                            WAVESHARE_EPD_HEIGHT,
                            kPortraitWidth,
                            kPortraitHeight,
                            snapshot.wifi_page,
                            snapshot.status_bar,
                            snapshot.global_footer);
}

void DrawTimeUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawTimePage(framebuffer,
                            WAVESHARE_EPD_WIDTH,
                            WAVESHARE_EPD_HEIGHT,
                            kPortraitWidth,
                            kPortraitHeight,
                            snapshot.time_page,
                            snapshot.status_bar,
                            snapshot.global_footer);
}

void DrawVibeCheckUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawVibeCheckPage(framebuffer,
                                 WAVESHARE_EPD_WIDTH,
                                 WAVESHARE_EPD_HEIGHT,
                                 kPortraitWidth,
                                 kPortraitHeight,
                                 snapshot.vibe_check_page,
                                 snapshot.status_bar,
                                 snapshot.global_footer);
}

void DrawSummarizeUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawSummarizePage(framebuffer,
                                 WAVESHARE_EPD_WIDTH,
                                 WAVESHARE_EPD_HEIGHT,
                                 kPortraitWidth,
                                 kPortraitHeight,
                                 snapshot.summarize_page,
                                 snapshot.status_bar,
                                 snapshot.global_footer);
}

void DrawNotesUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawNotesPage(framebuffer,
                             WAVESHARE_EPD_WIDTH,
                             WAVESHARE_EPD_HEIGHT,
                             kPortraitWidth,
                             kPortraitHeight,
                             snapshot.notes_page,
                             snapshot.status_bar,
                             snapshot.global_footer);
}

void DrawTodosUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawTodosPage(framebuffer,
                             WAVESHARE_EPD_WIDTH,
                             WAVESHARE_EPD_HEIGHT,
                             kPortraitWidth,
                             kPortraitHeight,
                             snapshot.todos_page,
                             snapshot.status_bar,
                             snapshot.global_footer);
}

void DrawFollowUpUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawFollowUpPage(framebuffer,
                                WAVESHARE_EPD_WIDTH,
                                WAVESHARE_EPD_HEIGHT,
                                kPortraitWidth,
                                kPortraitHeight,
                                snapshot.follow_up_page,
                                snapshot.status_bar,
                                snapshot.global_footer);
}

void DrawOnboardingUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawOnboardingPage(framebuffer,
                                  WAVESHARE_EPD_WIDTH,
                                  WAVESHARE_EPD_HEIGHT,
                                  kPortraitWidth,
                                  kPortraitHeight,
                                  snapshot.onboarding_page,
                                  snapshot.status_bar);
}

void DrawDetailsUnderlay(uint8_t* framebuffer, const RenderSnapshot& snapshot)
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    epaper_ui::DrawDetailsPage(framebuffer,
                               WAVESHARE_EPD_WIDTH,
                               WAVESHARE_EPD_HEIGHT,
                               kPortraitWidth,
                               kPortraitHeight,
                               snapshot.details_page,
                               snapshot.status_bar,
                               snapshot.global_footer);
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
        case RefreshMode::kFast:
            return "fast";
        case RefreshMode::kFull:
            return "full";
        default:
            return "unknown";
    }
}

const char* RefreshScopeName(RefreshScope scope)
{
    switch (scope) {
        case RefreshScope::kRegion:
            return "region";
        case RefreshScope::kScreen:
        default:
            return "screen";
    }
}

const char* OverlayRefreshPolicyName(OverlayRefreshPolicy policy)
{
    switch (policy) {
        case OverlayRefreshPolicy::kRebuildUnderlay:
            return "rebuild_underlay";
        case OverlayRefreshPolicy::kRebuildUnderlayFull:
            return "rebuild_underlay_full";
        case OverlayRefreshPolicy::kReuseUnderlaySnapshot:
            return "reuse_underlay_snapshot";
        default:
            return "unknown";
    }
}

esp_err_t RefreshCurrentScreenLocked(RefreshMode refresh_mode);

esp_err_t RefreshForMode(EpaperPanel& panel, RefreshMode refresh_mode)
{
    switch (refresh_mode) {
        case RefreshMode::kFull:
            return panel.RefreshFullBase();
        case RefreshMode::kFast:
            return panel.RefreshFastBase();
        case RefreshMode::kPartial:
        default:
            return panel.RefreshPartialFullScreen();
    }
}

esp_err_t ApplyHomeScreen(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawHomeUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kHome, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyLockScreen(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawLockScreenUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kLockScreen, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplySettings(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawSettingsUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kSettings, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyWifi(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawWifiUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kWifi, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyTime(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawTimeUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kTime, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyVibeCheck(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawVibeCheckUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kVibeCheck, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplySummarize(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawSummarizeUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kSummarize, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyNotes(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawNotesUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kNotes, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyTodos(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawTodosUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kTodos, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyFollowUp(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawFollowUpUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kFollowUp, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyOnboarding(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawOnboardingUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kOnboarding, std::memory_order_relaxed);
    return ESP_OK;
}

esp_err_t ApplyDetails(RefreshMode refresh_mode)
{
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    EpaperPanel& panel = Panel();
    DrawDetailsUnderlay(panel.framebuffer(), snapshot);
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = RefreshForMode(panel, refresh_mode);
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    s_current_screen.store(ScreenId::kDetails, std::memory_order_relaxed);
    return ESP_OK;
}

bool HasVisibleOverlay(const RenderSnapshot& snapshot)
{
    return snapshot.keyboard.visible || snapshot.card_modal.visible ||
           snapshot.select_modal.visible || snapshot.toast.visible;
}

esp_err_t EnqueueDisplayCommand(const DisplayCommand& incoming)
{
    if (s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_command_queue_mutex);

    const auto implies_full_refresh = [](const DisplayCommand& command) {
        switch (command.type) {
            case DisplayCommandType::kRefreshOverlay:
                return command.overlay_refresh_policy ==
                       OverlayRefreshPolicy::kRebuildUnderlayFull;
            case DisplayCommandType::kRefreshCurrent:
            case DisplayCommandType::kSetScreen:
                return command.refresh_request.refresh_mode != RefreshMode::kPartial;
            default:
                return false;
        }
    };

    DisplayCommand merged = incoming;
    DisplayCommand pending = {};
    if (xQueuePeek(s_command_queue, &pending, 0) == pdTRUE) {
        if (pending.type == DisplayCommandType::kRefreshCurrent &&
            incoming.type == DisplayCommandType::kRefreshCurrent &&
            pending.screen == incoming.screen) {
            merged.refresh_request =
                MergeRefreshRequest(pending.refresh_request, incoming.refresh_request);
        } else if (pending.type == DisplayCommandType::kRefreshOverlay &&
                   incoming.type == DisplayCommandType::kRefreshOverlay &&
                   pending.screen == incoming.screen) {
            merged.overlay_refresh_policy = MergeOverlayRefreshPolicy(
                pending.overlay_refresh_policy, incoming.overlay_refresh_policy);
        } else if (pending.type == DisplayCommandType::kSetScreen &&
                   incoming.type == DisplayCommandType::kSetScreen &&
                   pending.screen == incoming.screen) {
            // Same-screen set: keep the stronger refresh so a pending full refresh
            // (e.g. de-ghosting / base reseed) is not silently downgraded to partial.
            merged.refresh_request.refresh_mode = MergeRefreshMode(
                pending.refresh_request.refresh_mode, incoming.refresh_request.refresh_mode);
        } else if (pending.type == DisplayCommandType::kSetScreen &&
                   incoming.type != DisplayCommandType::kSetScreen) {
            // A queued screen change must not be dropped by a refresh of the (old)
            // current screen. The single-slot queue would otherwise let an incoming
            // refresh -- e.g. a status-bar/page refresh from an async event that
            // fires right after navigation (like a WiFi scan starting on wifi-page
            // entry) -- overwrite the pending kSetScreen, silently losing the
            // navigation. Keep the set-screen (it fully renders the new screen and
            // its overlays, subsuming the refresh) and carry the stronger mode.
            merged = pending;
            merged.refresh_request.refresh_mode = MergeRefreshMode(
                pending.refresh_request.refresh_mode, incoming.refresh_request.refresh_mode);
        }

        // The queue is a single slot, so xQueueOverwrite discards whatever is
        // pending. When the display task is busy (e.g. mid-refresh), a queued
        // FULL refresh (screen transition, wake,
        // de-ghost reseed) can be sitting here when a partial refresh of a
        // different command type arrives. The merges above only apply within a
        // command type, so a cross-type partial would silently drop the pending
        // full and leave ghosting. Never lose a queued full: promote the survivor
        // so a full-screen refresh still happens.
        if (implies_full_refresh(pending) && !implies_full_refresh(merged)) {
            if (merged.type == DisplayCommandType::kSetScreen) {
                // Keep the screen transition, just make it a full refresh.
                merged.refresh_request.refresh_mode = RefreshMode::kFull;
            } else {
                // A full-screen refresh of the current screen also repaints the
                // current overlays, so it safely subsumes a dropped overlay or
                // screen partial.
                merged.type = DisplayCommandType::kRefreshCurrent;
                merged.screen = s_current_screen.load(std::memory_order_relaxed);
                merged.refresh_request.refresh_mode = RefreshMode::kFull;
                merged.refresh_request.scope = RefreshScope::kScreen;
            }
        }
    }

    return xQueueOverwrite(s_command_queue, &merged) == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t RefreshCurrentScreenRegionLocked()
{
    // Renders the whole frame and lets the driver drive only the rendered pixel delta
    // (EpaperPanel::RefreshChangedRegion), skipping entirely when nothing changed. No
    // caller-supplied region is needed — the delta is derived from the framebuffer vs the
    // glass shadow, so a partial refresh can never strand a changed pixel.
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    if (HasVisibleOverlay(snapshot)) {
        return RefreshCurrentScreenLocked(RefreshMode::kPartial);
    }

    EpaperPanel& panel = Panel();
    switch (s_current_screen.load(std::memory_order_relaxed)) {
        case ScreenId::kHome:
            DrawHomeUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kSettings:
            DrawSettingsUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kWifi:
            DrawWifiUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kTime:
            DrawTimeUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kVibeCheck:
            DrawVibeCheckUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kSummarize:
            DrawSummarizeUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kNotes:
            DrawNotesUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kTodos:
            DrawTodosUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kFollowUp:
            DrawFollowUpUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kOnboarding:
            DrawOnboardingUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kDetails:
            DrawDetailsUnderlay(panel.framebuffer(), snapshot);
            break;
        case ScreenId::kLockScreen:
            DrawLockScreenUnderlay(panel.framebuffer(), snapshot);
            break;
        default:
            DrawHomeUnderlay(panel.framebuffer(), snapshot);
            break;
    }
    CaptureUnderlaySnapshot(panel.framebuffer());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = panel.RefreshChangedRegion();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

esp_err_t ApplyStartupSplash()
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);
    DrawSplashScreen(panel.framebuffer());

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = panel.RefreshFullBase();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

esp_err_t RefreshCurrentScreenLocked(RefreshMode refresh_mode)
{
    switch (s_current_screen.load(std::memory_order_relaxed)) {
        case ScreenId::kHome:
            return ApplyHomeScreen(refresh_mode);
        case ScreenId::kSettings:
            return ApplySettings(refresh_mode);
        case ScreenId::kWifi:
            return ApplyWifi(refresh_mode);
        case ScreenId::kTime:
            return ApplyTime(refresh_mode);
        case ScreenId::kVibeCheck:
            return ApplyVibeCheck(refresh_mode);
        case ScreenId::kSummarize:
            return ApplySummarize(refresh_mode);
        case ScreenId::kNotes:
            return ApplyNotes(refresh_mode);
        case ScreenId::kTodos:
            return ApplyTodos(refresh_mode);
        case ScreenId::kFollowUp:
            return ApplyFollowUp(refresh_mode);
        case ScreenId::kOnboarding:
            return ApplyOnboarding(refresh_mode);
        case ScreenId::kDetails:
            return ApplyDetails(refresh_mode);
        case ScreenId::kLockScreen:
            return ApplyLockScreen(refresh_mode);
        default:
            return ApplyHomeScreen(refresh_mode);
    }
}

esp_err_t RefreshOverlayStateLocked(OverlayRefreshPolicy policy)
{
    if (policy == OverlayRefreshPolicy::kRebuildUnderlay ||
        policy == OverlayRefreshPolicy::kRebuildUnderlayFull || !s_underlay_snapshot_valid) {
        const RefreshMode refresh_mode = policy == OverlayRefreshPolicy::kRebuildUnderlayFull
                                             ? RefreshMode::kFull
                                             : RefreshMode::kPartial;
        ESP_LOGI(kTag,
                 "Overlay refresh path: policy=%s snapshot_valid=%d full=%d",
                 OverlayRefreshPolicyName(policy),
                 s_underlay_snapshot_valid ? 1 : 0,
                 refresh_mode == RefreshMode::kFull ? 1 : 0);
        return RefreshCurrentScreenLocked(refresh_mode);
    }

    EpaperPanel& panel = Panel();
    const RenderSnapshot& snapshot = CaptureRenderSnapshot();
    ESP_LOGI(kTag,
             "Overlay refresh path: policy=%s snapshot_valid=%d",
             OverlayRefreshPolicyName(policy),
             s_underlay_snapshot_valid ? 1 : 0);
    std::memcpy(panel.framebuffer(), s_underlay_snapshot.data(), s_underlay_snapshot.size());
    DrawCurrentOverlays(panel.framebuffer(), snapshot);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = panel.RefreshPartialFullScreen();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

esp_err_t SleepPanelLocked(const char* reason)
{
    EpaperPanel& panel = Panel();
    ESP_RETURN_ON_ERROR(panel.Sleep(), kTag, "panel sleep failed");
    s_display_sleeping = true;
    ESP_LOGI(kTag, "Panel entered sleep for %s", reason);
    return ESP_OK;
}

void DisplayTask(void*)
{
    DisplayCommand command = {};
    command.type = DisplayCommandType::kSetScreen;
    command.screen = ScreenId::kHome;
    command.refresh_request.refresh_mode = RefreshMode::kPartial;
    while (true) {
        if (xQueueReceive(s_command_queue, &command, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(kTag,
                 "Display command requested: type=%d screen=%d mode=%s scope=%s source=%s",
                 static_cast<int>(command.type),
                 static_cast<int>(command.screen),
                 RefreshModeName(command.refresh_request.refresh_mode),
                 RefreshScopeName(command.refresh_request.scope),
                 command.source);
        std::lock_guard<std::mutex> lock(s_panel_mutex);
        if (s_display_sleeping) {
            if (command.type == DisplayCommandType::kSetScreen) {
                s_current_screen.store(command.screen, std::memory_order_relaxed);
            }
            ESP_LOGI(kTag, "Display command suppressed while display sleeping");
            continue;
        }

        esp_err_t err = ESP_OK;
        if (command.type == DisplayCommandType::kSetScreen) {
            if (command.screen == ScreenId::kLockScreen) {
                err = ApplyLockScreen(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kWifi) {
                err = ApplyWifi(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kSettings) {
                err = ApplySettings(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kTime) {
                err = ApplyTime(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kVibeCheck) {
                err = ApplyVibeCheck(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kSummarize) {
                err = ApplySummarize(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kNotes) {
                err = ApplyNotes(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kTodos) {
                err = ApplyTodos(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kFollowUp) {
                err = ApplyFollowUp(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kOnboarding) {
                err = ApplyOnboarding(command.refresh_request.refresh_mode);
            } else if (command.screen == ScreenId::kDetails) {
                err = ApplyDetails(command.refresh_request.refresh_mode);
            } else {
                err = ApplyHomeScreen(command.refresh_request.refresh_mode);
            }
        } else if (command.type == DisplayCommandType::kRefreshOverlay) {
            err = RefreshOverlayStateLocked(command.overlay_refresh_policy);
        } else {
            err = command.refresh_request.scope == RefreshScope::kRegion &&
                          command.refresh_request.refresh_mode == RefreshMode::kPartial
                      ? RefreshCurrentScreenRegionLocked()
                      : RefreshCurrentScreenLocked(
                            command.refresh_request.refresh_mode);
        }
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Display refresh failed (mode=%s): %s",
                     RefreshModeName(command.refresh_request.refresh_mode), esp_err_to_name(err));
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

RefreshMode MergeRefreshMode(RefreshMode lhs, RefreshMode rhs)
{
    return lhs == RefreshMode::kFull || rhs == RefreshMode::kFull ? RefreshMode::kFull
                                                                  : RefreshMode::kPartial;
}

RefreshRequest MergeRefreshRequest(const RefreshRequest& lhs, const RefreshRequest& rhs)
{
    RefreshRequest merged = {};
    merged.refresh_mode = MergeRefreshMode(lhs.refresh_mode, rhs.refresh_mode);
    // kScreen (always refresh) dominates kRegion (skip when unchanged); a full refresh
    // is always whole-screen.
    merged.scope = merged.refresh_mode == RefreshMode::kFull ||
                           lhs.scope == RefreshScope::kScreen ||
                           rhs.scope == RefreshScope::kScreen
                       ? RefreshScope::kScreen
                       : RefreshScope::kRegion;
    return merged;
}

OverlayRefreshPolicy MergeOverlayRefreshPolicy(OverlayRefreshPolicy lhs, OverlayRefreshPolicy rhs)
{
    // Strongest policy wins so a coalesced batch never downgrades a large-overlay
    // dismissal's full refresh: full > rebuild(partial) > reuse-snapshot.
    if (lhs == OverlayRefreshPolicy::kRebuildUnderlayFull ||
        rhs == OverlayRefreshPolicy::kRebuildUnderlayFull) {
        return OverlayRefreshPolicy::kRebuildUnderlayFull;
    }
    if (lhs == OverlayRefreshPolicy::kRebuildUnderlay ||
        rhs == OverlayRefreshPolicy::kRebuildUnderlay) {
        return OverlayRefreshPolicy::kRebuildUnderlay;
    }
    return OverlayRefreshPolicy::kReuseUnderlaySnapshot;
}

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    // The EPD owns SPI3 and is powered from the AXP2101 rails, so there is no
    // shared SPI bus to bring up and no GPIO power-enable — the panel initializes
    // its own bus in EpaperPanel::Initialize().

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
    return s_current_screen.load(std::memory_order_relaxed);
}

esp_err_t SetStatusBarState(const epaper_ui::StatusBarState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_status_bar_state = state;
    return ESP_OK;
}

esp_err_t SetGlobalFooterState(const epaper_ui::GlobalFooterState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_global_footer_state = state;
    return ESP_OK;
}

esp_err_t SetSettingsPageState(const epaper_ui::SettingsPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_settings_page_state = state;
    return ESP_OK;
}

esp_err_t SetWifiPageState(const epaper_ui::WifiPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_wifi_page_state = state;
    return ESP_OK;
}

esp_err_t SetTimePageState(const epaper_ui::TimePageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_time_page_state = state;
    return ESP_OK;
}

esp_err_t SetDashboardPageState(const epaper_ui::DashboardPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_dashboard_page_state = state;
    return ESP_OK;
}

esp_err_t SetVibeCheckPageState(const epaper_ui::VibeCheckPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_vibe_check_page_state = state;
    return ESP_OK;
}

esp_err_t SetSummarizePageState(const epaper_ui::SummarizePageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_summarize_page_state = state;
    return ESP_OK;
}

esp_err_t SetNotesPageState(const epaper_ui::NotesPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_notes_page_state = state;
    return ESP_OK;
}

esp_err_t SetTodosPageState(const epaper_ui::TodosPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_todos_page_state = state;
    return ESP_OK;
}

esp_err_t SetFollowUpPageState(const epaper_ui::FollowUpPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_follow_up_page_state = state;
    return ESP_OK;
}

esp_err_t SetOnboardingPageState(const epaper_ui::OnboardingPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_onboarding_page_state = state;
    return ESP_OK;
}

esp_err_t SetDetailsPageState(const epaper_ui::DetailsPageState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_details_page_state = state;
    return ESP_OK;
}

esp_err_t SetLockScreenState(const epaper_ui::LockScreenState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_lock_screen_state = state;
    return ESP_OK;
}

esp_err_t SetKeyboardState(const epaper_ui::KeyboardState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_keyboard_state = state;
    return ESP_OK;
}

esp_err_t SetCardModalState(const epaper_ui::CardModalState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_card_modal_state = state;
    return ESP_OK;
}

esp_err_t SetSelectModalState(const epaper_ui::SelectModalState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_select_modal_state = state;
    return ESP_OK;
}

esp_err_t SetToastState(const epaper_ui::ToastState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_toast_state = state;
    return ESP_OK;
}

esp_err_t SetStickyNoteState(const epaper_ui::StickyNoteState& state)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_sticky_note_state = state;
    return ESP_OK;
}

esp_err_t SetCurrentScreen(ScreenId screen, RefreshMode refresh_mode, const char* source)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kSetScreen;
    command.screen = screen;
    command.refresh_request.refresh_mode = refresh_mode;
    SetCommandSource(command, source);
    return EnqueueDisplayCommand(command);
}

esp_err_t RequestOverlayRefresh(OverlayRefreshPolicy policy, const char* source)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kRefreshOverlay;
    command.screen = s_current_screen.load(std::memory_order_relaxed);
    command.overlay_refresh_policy = policy;
    SetCommandSource(command, source);
    return EnqueueDisplayCommand(command);
}

esp_err_t RequestRefreshCurrentScreen(const RefreshRequest& refresh_request, const char* source)
{
    if (!s_initialized || s_command_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    DisplayCommand command = {};
    command.type = DisplayCommandType::kRefreshCurrent;
    command.screen = s_current_screen.load(std::memory_order_relaxed);
    command.refresh_request = refresh_request;
    SetCommandSource(command, source);
    return EnqueueDisplayCommand(command);
}

esp_err_t RequestRefreshCurrentScreen(RefreshMode refresh_mode, const char* source)
{
    return RequestRefreshCurrentScreen(
        RefreshRequest{
            .refresh_mode = refresh_mode,
        },
        source);
}

esp_err_t RefreshCurrentScreen(const RefreshRequest& refresh_request)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    if (s_display_sleeping) {
        return ESP_OK;
    }

    if (refresh_request.scope == RefreshScope::kRegion &&
        refresh_request.refresh_mode == RefreshMode::kPartial) {
        return RefreshCurrentScreenRegionLocked();
    }

    return RefreshCurrentScreenLocked(refresh_request.refresh_mode);
}

esp_err_t RefreshCurrentScreen(RefreshMode refresh_mode)
{
    return RefreshCurrentScreen(RefreshRequest{
        .refresh_mode = refresh_mode,
    });
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

    ESP_RETURN_ON_ERROR(RefreshCurrentScreenLocked(RefreshMode::kFull),
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
    ESP_RETURN_ON_ERROR(RefreshCurrentScreenLocked(RefreshMode::kFull),
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
