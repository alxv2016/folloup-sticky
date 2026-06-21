#include "ui_refresh_runtime.h"

#include <array>
#include <mutex>

#include "esp_check.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ui_refresh_runtime {
namespace {

constexpr const char* kTag = "UiRefreshRuntime";
constexpr uint32_t kUiRefreshTaskStackWords = 4096;

struct PendingSurface {
    ApplyCallback apply_callback = nullptr;
    display_service::RefreshMode refresh_mode = display_service::RefreshMode::kPartial;
    display_service::OverlayRefreshPolicy overlay_refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    bool request_overlay_refresh = false;
    bool pending = false;
};

constexpr size_t kSurfaceCount = 4;

std::mutex s_mutex;
std::array<PendingSurface, kSurfaceCount> s_pending = {};
TaskHandle_t s_task = nullptr;
bool s_initialized = false;

size_t SurfaceIndex(SurfaceKey key)
{
    switch (key) {
        case SurfaceKey::kOverlay:
            return 0;
        case SurfaceKey::kLockScreen:
            return 1;
        case SurfaceKey::kStatusBar:
            return 2;
        case SurfaceKey::kFooter:
            return 3;
        default:
            return 0;
    }
}

const char* SurfaceName(SurfaceKey key)
{
    switch (key) {
        case SurfaceKey::kOverlay:
            return "overlay";
        case SurfaceKey::kLockScreen:
            return "lock_screen";
        case SurfaceKey::kStatusBar:
            return "status_bar";
        case SurfaceKey::kFooter:
            return "footer";
        default:
            return "unknown";
    }
}

display_service::RefreshMode MergeRefreshMode(display_service::RefreshMode current,
                                              display_service::RefreshMode incoming)
{
    return current == display_service::RefreshMode::kFull ||
                   incoming == display_service::RefreshMode::kFull
               ? display_service::RefreshMode::kFull
               : display_service::RefreshMode::kPartial;
}

display_service::OverlayRefreshPolicy MergeOverlayRefreshPolicy(
    display_service::OverlayRefreshPolicy current,
    display_service::OverlayRefreshPolicy incoming)
{
    return current == display_service::OverlayRefreshPolicy::kRebuildUnderlay ||
                   incoming == display_service::OverlayRefreshPolicy::kRebuildUnderlay
               ? display_service::OverlayRefreshPolicy::kRebuildUnderlay
               : display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;
}

void UiRefreshTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            std::array<PendingSurface, kSurfaceCount> pending = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                if (!s_initialized) {
                    break;
                }
                pending = s_pending;
                for (PendingSurface& surface : s_pending) {
                    surface = {};
                }
            }

            bool any_pending = false;
            bool any_overlay_refresh = false;
            bool any_screen_refresh = false;
            display_service::RefreshMode merged_refresh_mode =
                display_service::RefreshMode::kPartial;
            display_service::OverlayRefreshPolicy merged_overlay_refresh_policy =
                display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;

            for (size_t index = 0; index < pending.size(); ++index) {
                const PendingSurface& surface = pending[index];
                if (!surface.pending) {
                    continue;
                }

                any_pending = true;
                if (surface.request_overlay_refresh) {
                    any_overlay_refresh = true;
                    merged_overlay_refresh_policy = MergeOverlayRefreshPolicy(
                        merged_overlay_refresh_policy, surface.overlay_refresh_policy);
                } else {
                    any_screen_refresh = true;
                    merged_refresh_mode =
                        MergeRefreshMode(merged_refresh_mode, surface.refresh_mode);
                }

                if (surface.apply_callback == nullptr) {
                    ESP_LOGD(kTag,
                             "Queued refresh-only dispatch: surface=%zu mode=%d overlay=%d",
                             index,
                             static_cast<int>(surface.refresh_mode),
                             surface.request_overlay_refresh ? 1 : 0);
                    continue;
                }

                const esp_err_t err = surface.apply_callback();
                if (err == ESP_OK) {
                    ESP_LOGD(kTag,
                             "Applied UI surface refresh: surface=%zu mode=%d",
                             index,
                             static_cast<int>(surface.refresh_mode));
                    continue;
                }

                if (err != ESP_ERR_INVALID_STATE) {
                    ESP_LOGW(kTag,
                             "UI surface apply failed: surface=%zu mode=%d err=%s",
                             index,
                             static_cast<int>(surface.refresh_mode),
                             esp_err_to_name(err));
                }
            }

            if (!any_pending) {
                break;
            }

            if (display_service::IsInitialized()) {
                esp_err_t err = ESP_OK;
                if (any_screen_refresh) {
                    err = display_service::RequestRefreshCurrentScreen(merged_refresh_mode);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(kTag,
                                 "UI screen refresh request failed: mode=%d err=%s",
                                 static_cast<int>(merged_refresh_mode),
                                 esp_err_to_name(err));
                    }
                } else if (any_overlay_refresh) {
                    err = display_service::RequestOverlayRefresh(merged_overlay_refresh_policy);
                    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                        ESP_LOGW(kTag,
                                 "UI overlay refresh request failed: policy=%d err=%s",
                                 static_cast<int>(merged_overlay_refresh_policy),
                                 esp_err_to_name(err));
                    }
                }
            }

            if (ulTaskNotifyTake(pdTRUE, 0) == 0) {
                break;
            }
        }
    }
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) {
        return ESP_OK;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(UiRefreshTask,
                                                       "ui_refresh",
                                                       kUiRefreshTaskStackWords,
                                                       nullptr,
                                                       followup_task_config::kPriorityUiRefresh,
                                                       &s_task,
                                                       followup_task_config::kAppCore);
    if (created != pdPASS) {
        s_task = nullptr;
        return ESP_ERR_NO_MEM;
    }

    s_pending = {};
    s_initialized = true;
    ESP_LOGI(kTag, "UI refresh runtime initialized");
    return ESP_OK;
}

bool IsInitialized()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_initialized;
}

esp_err_t Schedule(SurfaceKey key,
                   ApplyCallback apply_callback,
                   display_service::RefreshMode refresh_mode)
{
    TaskHandle_t task = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized || s_task == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }

        PendingSurface& pending = s_pending[SurfaceIndex(key)];
        pending.apply_callback = apply_callback;
        pending.refresh_mode = pending.pending
                                   ? MergeRefreshMode(pending.refresh_mode, refresh_mode)
                                   : refresh_mode;
        pending.overlay_refresh_policy = display_service::OverlayRefreshPolicy::kRebuildUnderlay;
        pending.request_overlay_refresh = false;
        pending.pending = true;
        task = s_task;
    }

    ESP_LOGD(kTag,
             "Queued UI refresh: surface=%s mode=%d apply=%d",
             SurfaceName(key),
             static_cast<int>(refresh_mode),
             apply_callback != nullptr ? 1 : 0);
    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t ScheduleOverlay(SurfaceKey key,
                          ApplyCallback apply_callback,
                          display_service::OverlayRefreshPolicy policy)
{
    TaskHandle_t task = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized || s_task == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }

        PendingSurface& pending = s_pending[SurfaceIndex(key)];
        pending.apply_callback = apply_callback;
        pending.refresh_mode = display_service::RefreshMode::kPartial;
        pending.overlay_refresh_policy = pending.pending && pending.request_overlay_refresh
                                             ? MergeOverlayRefreshPolicy(
                                                   pending.overlay_refresh_policy, policy)
                                             : policy;
        pending.request_overlay_refresh = true;
        pending.pending = true;
        task = s_task;
    }

    ESP_LOGD(kTag,
             "Queued overlay refresh: surface=%s policy=%d apply=%d",
             SurfaceName(key),
             static_cast<int>(policy),
             apply_callback != nullptr ? 1 : 0);
    xTaskNotifyGive(task);
    return ESP_OK;
}

esp_err_t RequestRefresh(SurfaceKey key, display_service::RefreshMode refresh_mode)
{
    return Schedule(key, nullptr, refresh_mode);
}

}  // namespace ui_refresh_runtime
