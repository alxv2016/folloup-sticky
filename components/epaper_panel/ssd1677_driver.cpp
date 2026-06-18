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
    ESP_RETURN_ON_ERROR(HardwareReset(), kTag, "partial hardware reset failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait before partial init failed");

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
    ESP_RETURN_ON_ERROR(SendData(0xF7), kTag, "display update control data failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x20), kTag, "master activation command failed");
    ESP_RETURN_ON_ERROR(ReadBusy(), kTag, "busy wait after display update failed");
    metrics_.trigger_us = esp_timer_get_time() - start_us;
    return ESP_OK;
}

esp_err_t EpaperPanel::TurnOnDisplayPart()
{
    const int64_t start_us = esp_timer_get_time();
    ESP_RETURN_ON_ERROR(SendCommand(0x22), kTag, "partial update control command failed");
    ESP_RETURN_ON_ERROR(SendData(0xFF), kTag, "partial update control data failed");
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

void EpaperPanel::CopyFramebufferRegionToPrevious(uint16_t x_start, uint16_t y_start,
                                                  uint16_t x_end, uint16_t y_end)
{
    if (framebuffer_ == nullptr || previous_framebuffer_ == nullptr) {
        return;
    }

    const size_t mono_stride = static_cast<size_t>(width_) / 8U;
    const size_t row_bytes = static_cast<size_t>(x_end - x_start) / 8U;
    for (uint16_t row = y_start; row < y_end; ++row) {
        const size_t row_offset = static_cast<size_t>(row) * mono_stride + (x_start / 8U);
        memcpy(previous_framebuffer_ + row_offset, framebuffer_ + row_offset, row_bytes);
    }
}

esp_err_t EpaperPanel::WriteRegionBytes(const uint8_t* data, uint16_t x_start, uint16_t y_start,
                                        uint16_t x_end, uint16_t y_end)
{
    if (data == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (x_start >= x_end || y_start >= y_end) {
        return ESP_OK;
    }

    const size_t mono_stride = static_cast<size_t>(width_) / 8U;
    const int row_bytes = static_cast<int>((x_end - x_start) / 8U);
    const size_t byte_x = x_start / 8U;

    SetDc(1);
    SetCs(0);
    esp_err_t err = ESP_OK;
    for (uint16_t row = y_start; row < y_end; ++row) {
        const uint8_t* row_data = data + static_cast<size_t>(row) * mono_stride + byte_x;
        err = TransmitBytes(row_data, row_bytes);
        if (err != ESP_OK) {
            break;
        }
    }
    SetCs(1);
    return err;
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

esp_err_t EpaperPanel::RefreshPartialRegion(uint16_t raw_x_start, uint16_t raw_y_start,
                                            uint16_t raw_x_end, uint16_t raw_y_end)
{
    raw_x_start = std::min<uint16_t>(raw_x_start, static_cast<uint16_t>(width_));
    raw_x_end = std::min<uint16_t>(raw_x_end, static_cast<uint16_t>(width_));
    raw_y_start = std::min<uint16_t>(raw_y_start, static_cast<uint16_t>(height_));
    raw_y_end = std::min<uint16_t>(raw_y_end, static_cast<uint16_t>(height_));
    if (raw_x_start >= raw_x_end || raw_y_start >= raw_y_end) {
        return ESP_OK;
    }

    raw_x_start &= static_cast<uint16_t>(~0x07U);
    raw_x_end = static_cast<uint16_t>((raw_x_end + 7U) & ~0x07U);
    raw_x_end = std::min<uint16_t>(raw_x_end, static_cast<uint16_t>(width_));
    if (raw_x_start >= raw_x_end) {
        return ESP_OK;
    }

    if (!CanPartialRefresh(kMaxPartialRefreshesBeforeFull)) {
        return RefreshFullBase();
    }
    if (framebuffer_ == nullptr || previous_framebuffer_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint16_t window_x_start = raw_x_start;
    const uint16_t window_x_end = static_cast<uint16_t>(raw_x_end - 1U);
    const uint16_t window_y_start = static_cast<uint16_t>(raw_y_end - 1U);
    const uint16_t window_y_end = raw_y_start;
    const uint16_t cursor_x = raw_x_start;
    const uint16_t cursor_y = window_y_start;

    ResetMetrics();
    ESP_RETURN_ON_ERROR(InitPartial(), kTag, "partial init failed");
    ESP_RETURN_ON_ERROR(SetWindow(window_x_start, window_y_start, window_x_end, window_y_end),
                        kTag, "partial region RAM window failed");
    ESP_RETURN_ON_ERROR(SetCursor(cursor_x, cursor_y), kTag, "partial region RAM cursor failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x24), kTag, "write partial current image command failed");
    ESP_RETURN_ON_ERROR(WriteRegionBytes(framebuffer_, raw_x_start, raw_y_start, raw_x_end,
                                         raw_y_end),
                        kTag, "write partial current image failed");
    ESP_RETURN_ON_ERROR(SetCursor(cursor_x, cursor_y),
                        kTag, "reset partial previous RAM cursor failed");
    ESP_RETURN_ON_ERROR(SendCommand(0x26), kTag, "write partial previous image command failed");
    ESP_RETURN_ON_ERROR(WriteRegionBytes(previous_framebuffer_, raw_x_start, raw_y_start,
                                         raw_x_end, raw_y_end),
                        kTag, "write partial previous image failed");
    ESP_RETURN_ON_ERROR(TurnOnDisplayPart(), kTag, "partial display update failed");
    CopyFramebufferRegionToPrevious(raw_x_start, raw_y_start, raw_x_end, raw_y_end);
    wake_refresh_pending_ = false;
    ++partial_refresh_count_;
    state_ = EpaperPanelState::kActive;
    return ESP_OK;
}

esp_err_t EpaperPanel::RefreshPartialFullScreen()
{
    return RefreshPartialRegion(0, 0, static_cast<uint16_t>(width_),
                                static_cast<uint16_t>(height_));
}

esp_err_t EpaperPanel::Sleep()
{
    ESP_RETURN_ON_ERROR(SendCommand(0x10), kTag, "deep sleep command failed");
    ESP_RETURN_ON_ERROR(SendData(0x01), kTag, "deep sleep data failed");
    vTaskDelay(pdMS_TO_TICKS(10));
    SetRst(0);
    SetCs(0);
    SetDc(0);
    state_ = EpaperPanelState::kDeepSleep;
    base_image_initialized_ = false;
    wake_refresh_pending_ = false;
    partial_refresh_count_ = 0;
    return ESP_OK;
}
