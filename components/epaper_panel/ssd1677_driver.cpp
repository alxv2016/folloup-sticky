#include "epaper_panel.h"

#include <algorithm>
#include <cstring>

#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* kTag = "Ssd1677";
constexpr int kMaxPartialRefreshesBeforeFull = 20;
// Display Update Control 2 (0x22) sequences, matching the `followup` esp-epaper driver
// that is proven on this panel: 0xF7 drives the mode-1 full waveform, 0xFF the mode-2
// differential waveform used for partials.
//
// Display Update Control 1 (0x21) is deliberately never written. Its reset default is
// "normal" for both RAM planes, which is what makes the panel compare 0x24 against 0x26
// and move only the changed pixels. Writing it is also load-bearing in the wrong
// direction here: InitPartial intentionally skips the hardware reset, so any value left
// in 0x21 by a preceding full refresh would carry into the next partial.
constexpr uint8_t kDisplayUpdateCtrl2Full = 0xF7;
constexpr uint8_t kDisplayUpdateCtrl2Partial = 0xFF;

}  // namespace

esp_err_t EpaperPanel::SetWindow(uint16_t x_start, uint16_t y_start, uint16_t x_end,
                                 uint16_t y_end)
{
    const uint8_t ram_x_window[] = {
        static_cast<uint8_t>(x_start & 0xFF),
        static_cast<uint8_t>(x_start >> 8),
        static_cast<uint8_t>(x_end & 0xFF),
        static_cast<uint8_t>(x_end >> 8),
    };
    const uint8_t ram_y_window[] = {
        static_cast<uint8_t>(y_start & 0xFF),
        static_cast<uint8_t>(y_start >> 8),
        static_cast<uint8_t>(y_end & 0xFF),
        static_cast<uint8_t>(y_end >> 8),
    };

    ESP_RETURN_ON_ERROR(SendCommandWithData(0x44, ram_x_window, sizeof(ram_x_window)),
                        kTag, "RAM X window failed");
    return SendCommandWithData(0x45, ram_y_window, sizeof(ram_y_window));
}

esp_err_t EpaperPanel::SetCursor(uint16_t x_start, uint16_t y_start)
{
    const uint8_t cursor_x[] = {
        static_cast<uint8_t>(x_start & 0xFF),
        static_cast<uint8_t>(x_start >> 8),
    };
    const uint8_t cursor_y[] = {
        static_cast<uint8_t>(y_start & 0xFF),
        static_cast<uint8_t>(y_start >> 8),
    };

    ESP_RETURN_ON_ERROR(SendCommandWithData(0x4E, cursor_x, sizeof(cursor_x)),
                        kTag, "RAM X cursor failed");
    return SendCommandWithData(0x4F, cursor_y, sizeof(cursor_y));
}

esp_err_t EpaperPanel::InitFull()
{
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(HardwareReset(), kTag, "hardware reset failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait before software reset failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x12), kTag, "software reset command failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait after software reset failed");

    const uint8_t booster_soft_start[] = {0xAE, 0xC7, 0xC3, 0xC0, 0x80};
    const uint8_t driver_output[] = {
        static_cast<uint8_t>((height_ - 1) & 0xFF),
        static_cast<uint8_t>((height_ - 1) >> 8),
        0x02,
    };
    const uint8_t data_entry_mode[] = {0x01};
    const uint8_t border_waveform[] = {0x01};
    const uint8_t temperature_select[] = {0x80};
    const uint16_t full_x_start = 0;
    const uint16_t full_x_end = static_cast<uint16_t>(width_ - 1);
    const uint16_t full_y_start = static_cast<uint16_t>(height_ - 1);
    const uint16_t full_y_end = 0;

    ESP_RETURN_ON_ERROR(SendCommandWithData(0x0C, booster_soft_start, sizeof(booster_soft_start)),
                        kTag, "booster soft-start failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x01, driver_output, sizeof(driver_output)),
                        kTag, "driver output failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x11, data_entry_mode, sizeof(data_entry_mode)),
                        kTag, "data entry mode failed");
    ESP_RETURN_ON_ERROR(SetWindow(full_x_start, full_y_start, full_x_end, full_y_end),
                        kTag, "full RAM window failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x3C, border_waveform, sizeof(border_waveform)),
                        kTag, "border waveform failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x18, temperature_select, sizeof(temperature_select)),
                        kTag, "temperature selection failed");
    ESP_RETURN_ON_ERROR(SetCursor(full_x_start, full_y_start), kTag, "full RAM cursor failed");

    metrics_.init_ready_us = metrics_.panel_busy_us;
    state_ = EpaperPanelState::kActive;
    return ESP_OK;
}

esp_err_t EpaperPanel::InitPartial()
{
    // No hardware reset per partial. The datasheet reference sequence resets the IC
    // only once at power-on; a partial runs against the panel established by the last
    // full refresh (InitFull still resets). Resetting before every partial cost ~52ms
    // (reset_high_ms + busy wait) for no benefit now that partials rewrite both RAM
    // buffers in full. The registers below are re-asserted (cheap, no delays) because
    // a partial uses a different border waveform than the full refresh.
    const uint8_t driver_output[] = {
        static_cast<uint8_t>((height_ - 1) & 0xFF),
        static_cast<uint8_t>((height_ - 1) >> 8),
        0x02,
    };
    const uint8_t data_entry_mode[] = {0x01};
    const uint8_t border_waveform[] = {0x80};
    const uint8_t temperature_select[] = {0x80};
    const uint16_t full_x_start = 0;
    const uint16_t full_x_end = static_cast<uint16_t>(width_ - 1);
    const uint16_t full_y_start = static_cast<uint16_t>(height_ - 1);
    const uint16_t full_y_end = 0;

    ESP_RETURN_ON_ERROR(SendCommandWithData(0x01, driver_output, sizeof(driver_output)),
                        kTag, "partial driver output failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x11, data_entry_mode, sizeof(data_entry_mode)),
                        kTag, "partial data entry mode failed");
    ESP_RETURN_ON_ERROR(SetWindow(full_x_start, full_y_start, full_x_end, full_y_end),
                        kTag, "partial RAM window failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x3C, border_waveform, sizeof(border_waveform)),
                        kTag, "partial border waveform failed");
    ESP_RETURN_ON_ERROR(SendCommandWithData(0x18, temperature_select, sizeof(temperature_select)),
                        kTag, "partial temperature selection failed");
    ESP_RETURN_ON_ERROR(SetCursor(full_x_start, full_y_start), kTag, "partial RAM cursor failed");

    state_ = EpaperPanelState::kActive;
    return ESP_OK;
}

esp_err_t EpaperPanel::DisplayFullBase()
{
    if (framebuffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(SendCommand(0x24), kTag, "write full current image command failed");
    ESP_RETURN_ON_ERROR(WriteBytes(framebuffer_, config_.buffer_len),
                        kTag, "write full current image failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x26), kTag, "write full previous image command failed");
    return WriteBytes(framebuffer_, config_.buffer_len);
}

esp_err_t EpaperPanel::TurnOnDisplay()
{
    const int64_t start_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(SendCommand(0x22), kTag, "display update control command failed");
    ESP_RETURN_ON_ERROR(SendData(kDisplayUpdateCtrl2Full), kTag, "display update control 2 full data failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x20), kTag, "master activation command failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait after display update failed");
    metrics_.trigger_us = esp_timer_get_time() - start_us;
    return ESP_OK;
}

esp_err_t EpaperPanel::TurnOnDisplayPart()
{
    const int64_t start_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(SendCommand(0x22), kTag, "partial update control command failed");
    ESP_RETURN_ON_ERROR(SendData(kDisplayUpdateCtrl2Partial), kTag, "partial update control 2 data failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x20), kTag, "partial master activation command failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait after partial update failed");
    metrics_.trigger_us = esp_timer_get_time() - start_us;
    return ESP_OK;
}

void EpaperPanel::CopyFramebufferToPrevious()
{
    if (framebuffer_ != nullptr && previous_framebuffer_ != nullptr) {
        memcpy(previous_framebuffer_, framebuffer_, static_cast<size_t>(config_.buffer_len));
    }
}

esp_err_t EpaperPanel::RefreshFull()
{
    return RefreshFullBase();
}

esp_err_t EpaperPanel::RefreshFullBase()
{
    ResetMetrics();
    ESP_RETURN_ON_ERROR(InitFull(), kTag, "full init failed");
    ESP_RETURN_ON_ERROR(DisplayFullBase(), kTag, "full base image write failed");
    ESP_RETURN_ON_ERROR(TurnOnDisplay(), kTag, "full base display update failed");
    CopyFramebufferToPrevious();
    base_image_initialized_ = true;
    wake_refresh_pending_ = false;
    partial_refresh_count_ = 0;
    state_ = EpaperPanelState::kActive;
    return ESP_OK;
}

esp_err_t EpaperPanel::RefreshPartialFullScreen()
{
    if (!CanPartialRefresh(kMaxPartialRefreshesBeforeFull)) {
        return RefreshFullBase();
    }
    if (framebuffer_ == nullptr || previous_framebuffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Whole-screen partial. Windowed/region partial is NOT possible on this SSD1677: the
    // activation drives the WHOLE panel from 0x24, the RAM window registers only scope
    // writes (not the drive), and nothing limits the drive to a window — a windowed 0x24
    // write leaves previously-touched regions outside the window re-energizing stale
    // content. So both RAM planes are rewritten in full every cycle. The partial waveform
    // still physically moves only the pixels where 0x24 != 0x26, so the update shows just
    // the changed element with no full-screen flash. (RefreshChangedRegion does the pixel
    // diff and skips this call entirely when nothing changed.)
    //
    // RAM addressing matches DisplayFullBase: data-entry mode 0x01 decrements Y from a
    // top-corner cursor, so framebuffer row r lands on gate line (height-1-r). The window
    // spans the whole panel: X [0, width-1], Y [height-1, 0], cursor at (0, height-1).
    const uint16_t window_x_end = static_cast<uint16_t>(width_ - 1);
    const uint16_t window_y_start = static_cast<uint16_t>(height_ - 1);

    ResetMetrics();
    ESP_RETURN_ON_ERROR(InitPartial(), kTag, "partial init failed");

    // OLD (0x26): rewrite the full glass shadow so untouched regions are not re-energized.
    // previous_framebuffer_ tracks the glass. InitPartial left the full-panel window/cursor
    // set, mirroring DisplayFullBase.
    ESP_RETURN_ON_ERROR(SendCommand(0x26), kTag, "write partial previous image command failed");
    ESP_RETURN_ON_ERROR(WriteBytes(previous_framebuffer_, config_.buffer_len),
                        kTag, "write partial previous image failed");

    // NEW (0x24): rewrite the full frame. The 0x26 write above advanced the RAM address
    // counter, so reset the full-panel window/cursor before streaming 0x24.
    ESP_RETURN_ON_ERROR(SetWindow(0, window_y_start, window_x_end, 0),
                        kTag, "partial RAM window failed");
    ESP_RETURN_ON_ERROR(SetCursor(0, window_y_start), kTag, "partial RAM cursor failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x24), kTag, "write partial current image command failed");
    ESP_RETURN_ON_ERROR(WriteBytes(framebuffer_, config_.buffer_len),
                        kTag, "write partial current image failed");
    ESP_RETURN_ON_ERROR(TurnOnDisplayPart(), kTag, "partial display update failed");

    CopyFramebufferToPrevious();
    wake_refresh_pending_ = false;
    ++partial_refresh_count_;
    state_ = EpaperPanelState::kActive;
    return ESP_OK;
}

esp_err_t EpaperPanel::RefreshChangedRegion()
{
    if (framebuffer_ == nullptr || previous_framebuffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    // Skip when nothing changed; otherwise drive a whole-screen partial. Proven on
    // this SSD1677 panel (empirically, via the datasheet, and via a from-scratch
    // isolation test): the activation drives the WHOLE panel from 0x24, the window
    // registers only scope RAM writes (not the drive), and there is no register to
    // limit the drive to a window. So a windowed 0x24 write leaves previously-touched
    // regions outside the new window re-energizing stale content. Only a full 0x24
    // write is coherent. The partial waveform still moves only the pixels where
    // 0x24 != 0x26, so this updates just the changed element with no full-screen flash.
    if (memcmp(framebuffer_, previous_framebuffer_,
               static_cast<size_t>(config_.buffer_len)) == 0) {
        return ESP_OK;
    }
    return RefreshPartialFullScreen();
}

esp_err_t EpaperPanel::Sleep()
{
    ESP_RETURN_ON_ERROR(SendCommand(0x10), kTag, "deep sleep command failed");
    ESP_RETURN_ON_ERROR(SendData(0x01), kTag, "deep sleep data failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    SetRst(0);
    // Leave CS deasserted after entering deep sleep (EPD owns SPI3 alone).
    SetCs(1);
    SetDc(0);
    state_ = EpaperPanelState::kDeepSleep;
    base_image_initialized_ = false;
    wake_refresh_pending_ = false;
    partial_refresh_count_ = 0;
    return ESP_OK;
}
