#include "display_service.h"

#include <atomic>
#include <mutex>

#include "epaper_panel.h"
#include "esp_check.h"
#include "esp_log.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace display_service {
namespace {

constexpr const char* kTag = "DisplayService";

bool s_initialized = false;
std::mutex s_panel_mutex;
bool s_display_sleeping = false;
std::atomic<bool> s_refresh_in_progress = false;

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

esp_err_t RefreshBlankSurface()
{
    EpaperPanel& panel = Panel();
    panel.Clear(true);

    RefreshBusyGuard refresh_busy;
    const esp_err_t err = panel.RefreshFullBase();
    if (err != ESP_OK) {
        return err;
    }

    LogMetrics(panel.metrics());
    return ESP_OK;
}

esp_err_t RefreshBlankAndSleep()
{
    ESP_RETURN_ON_ERROR(RefreshBlankSurface(), kTag, "blank sleep refresh failed");
    EpaperPanel& panel = Panel();
    ESP_RETURN_ON_ERROR(panel.Sleep(), kTag, "panel sleep failed");
    s_display_sleeping = true;
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
    {
        std::lock_guard<std::mutex> lock(s_panel_mutex);
        ESP_RETURN_ON_ERROR(RefreshBlankSurface(),
                            kTag,
                            "blank panel refresh failed");
    }

    s_initialized = true;
    ESP_LOGI(kTag, "Display initialized with blank screen");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
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
    ESP_RETURN_ON_ERROR(RefreshBlankAndSleep(),
                        kTag,
                        "display blank sleep refresh failed");
    ESP_LOGI(kTag, "Display entered sleep with blank screen");
    return ESP_OK;
}

esp_err_t EnterLightSleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    ESP_RETURN_ON_ERROR(RefreshBlankAndSleep(),
                        kTag,
                        "light sleep blank refresh failed");
    ESP_LOGI(kTag, "Display prepared for light sleep with blank screen");
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

    ESP_RETURN_ON_ERROR(RefreshBlankSurface(),
                        kTag,
                        "display blank wake refresh failed");
    s_display_sleeping = false;
    ESP_LOGI(kTag, "Display woke with blank full refresh");
    return ESP_OK;
}

esp_err_t RecoverAfterLightSleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    std::lock_guard<std::mutex> lock(s_panel_mutex);
    ESP_RETURN_ON_ERROR(RefreshBlankSurface(),
                        kTag,
                        "display blank light-sleep recovery refresh failed");
    s_display_sleeping = false;
    ESP_LOGI(kTag, "Display recovered after light sleep with blank full refresh");
    return ESP_OK;
}

bool IsRefreshInProgress()
{
    return s_refresh_in_progress.load(std::memory_order_relaxed);
}

}  // namespace display_service
