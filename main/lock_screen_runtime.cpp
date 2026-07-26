#include "lock_screen_runtime.h"

#include <climits>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>

#include "display_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "status_bar_runtime.h"
#include "ui_refresh_runtime.h"

namespace lock_screen_runtime {
namespace {

constexpr const char* kTag = "LockScreenRuntime";
constexpr time_t kMinValidEpoch = 1600000000;
constexpr uint64_t kClockPollPeriodUs = 1000 * 1000;

std::mutex s_mutex;
bool s_initialized = false;
bool s_active = false;
display_service::ScreenId s_restore_screen = display_service::ScreenId::kHome;
esp_timer_handle_t s_clock_timer = nullptr;
uint32_t s_last_minute_key = UINT_MAX;
epaper_ui::LockScreenState s_state = {};

uint32_t BuildMinuteKey(time_t now)
{
    if (now < kMinValidEpoch) {
        return UINT_MAX;
    }
    return static_cast<uint32_t>(now / 60);
}

int FormatHour12(int hour24)
{
    const int hour12 = hour24 % 12;
    return hour12 == 0 ? 12 : hour12;
}

bool RebuildClockStateLocked(bool force)
{
    const time_t now = time(nullptr);
    const uint32_t minute_key = BuildMinuteKey(now);
    if (!force && minute_key == s_last_minute_key) {
        return false;
    }

    epaper_ui::LockScreenState next = {};
    if (now >= kMinValidEpoch) {
        std::tm local_tm = {};
        localtime_r(&now, &local_tm);

        // Sized for the widest %02d the compiler must assume (an int can print more than
        // two digits), not just the 00-59 these actually produce. -Os turns the narrower
        // buffers into a format-truncation error.
        char hour_text[12] = {};
        char minute_text[12] = {};
        char weekday_text[16] = {};
        char month_text[8] = {};

        std::snprintf(hour_text, sizeof(hour_text), "%02d", FormatHour12(local_tm.tm_hour));
        std::snprintf(minute_text, sizeof(minute_text), "%02d", local_tm.tm_min);
        strftime(weekday_text, sizeof(weekday_text), "%A", &local_tm);
        strftime(month_text, sizeof(month_text), "%b", &local_tm);

        next.hour_text = hour_text;
        next.minute_text = minute_text;
        next.weekday_text = weekday_text;
        next.date_text =
            std::string(month_text) + " " + std::to_string(local_tm.tm_mday) + ", " +
            std::to_string(local_tm.tm_year + 1900);
    }

    const bool changed = force || next.hour_text != s_state.hour_text ||
                         next.minute_text != s_state.minute_text ||
                         next.weekday_text != s_state.weekday_text ||
                         next.date_text != s_state.date_text;
    if (!changed) {
        s_last_minute_key = minute_key;
        return false;
    }

    s_state = std::move(next);
    s_last_minute_key = minute_key;
    return true;
}

esp_err_t UpdateDisplayState()
{
    epaper_ui::LockScreenState state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        state = s_state;
    }

    return display_service::SetLockScreenState(state);
}

esp_err_t PushState(epaper_ui::LockScreenState state,
                    bool active,
                    bool request_refresh_if_active)
{
    ESP_RETURN_ON_ERROR(display_service::SetLockScreenState(state),
                        kTag,
                        "set lock screen state failed");
    if (active && request_refresh_if_active) {
        return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kLockScreen,
                                            &UpdateDisplayState,
                                            display_service::RefreshMode::kPartial);
    }
    return ESP_OK;
}

void OnClockTimer(void*)
{
    (void)SyncClockState(false);
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) {
        return ESP_OK;
    }

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &OnClockTimer;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "lock_screen";
    timer_args.skip_unhandled_events = true;

    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_clock_timer),
                        kTag,
                        "clock timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_clock_timer, kClockPollPeriodUs),
                        kTag,
                        "clock timer start failed");

    s_initialized = true;
    ESP_LOGI(kTag, "Lock screen runtime initialized");
    return ESP_OK;
}

bool IsActive()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_active;
}

esp_err_t Show()
{
    epaper_ui::LockScreenState state = {};
    bool already_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        already_active = s_active;
        if (!s_active) {
            const display_service::ScreenId current_screen = display_service::GetCurrentScreen();
            if (current_screen != display_service::ScreenId::kLockScreen) {
                s_restore_screen = current_screen;
            }
        }
        (void)RebuildClockStateLocked(true);
        s_active = true;
        state = s_state;
    }

    ESP_RETURN_ON_ERROR(PushState(state, false, false), kTag, "push lock state failed");
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status row update before lock screen show failed: %s",
                 esp_err_to_name(status_bar_err));
    }
    const esp_err_t err =
        display_service::SetCurrentScreen(display_service::ScreenId::kLockScreen,
                                          display_service::RefreshMode::kFull,
                                          "lock_screen_show");
    if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_active = already_active;
    }
    return err;
}

esp_err_t Hide()
{
    display_service::ScreenId restore_screen = display_service::ScreenId::kHome;
    bool was_active = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        if (!s_active) {
            return ESP_OK;
        }
        was_active = s_active;
        s_active = false;
        restore_screen = s_restore_screen;
    }

    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update before lock screen hide failed: %s",
                 esp_err_to_name(status_bar_err));
    }
    const esp_err_t err = display_service::SetCurrentScreen(restore_screen,
                                                            display_service::RefreshMode::kFull,
                                                            "lock_screen_hide");
    if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_active = was_active;
    }
    return err;
}

esp_err_t Toggle()
{
    return IsActive() ? Hide() : Show();
}

esp_err_t RequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::RequestRefresh(ui_refresh_runtime::SurfaceKey::kLockScreen,
                                              refresh_mode);
}

esp_err_t SyncClockState(bool request_refresh_if_active)
{
    epaper_ui::LockScreenState state = {};
    bool active = false;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        active = s_active;
        if (!active && !request_refresh_if_active) {
            return ESP_OK;
        }
        changed = RebuildClockStateLocked(request_refresh_if_active);
        if (!changed) {
            return ESP_OK;
        }
        state = s_state;
    }

    return PushState(state, active, request_refresh_if_active);
}

}  // namespace lock_screen_runtime
