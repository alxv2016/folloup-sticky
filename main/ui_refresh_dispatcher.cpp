#include "ui_refresh_dispatcher.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>

#include "esp_err.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ui_refresh_dispatcher {
namespace {

constexpr const char* kTag = "UiRefreshDispatch";
constexpr const char* kTaskName = "ui_refresh_dispatch";
constexpr uint32_t kTaskStackWords = 4096;
constexpr size_t kMaxPendingCallbacks = 16;

struct PendingCallback {
    std::function<void()> callback = {};
    bool keyed = false;
    uint32_t key = 0;
};

std::mutex s_mutex;
std::deque<PendingCallback> s_callbacks = {};
TaskHandle_t s_task = nullptr;
size_t s_dropped_callback_count = 0;

void WorkerTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            std::function<void()> callback = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                if (s_callbacks.empty()) {
                    if (s_dropped_callback_count > 0) {
                        ESP_LOGW(kTag,
                                 "Dropped %u stale UI refresh callbacks while saturated",
                                 static_cast<unsigned>(s_dropped_callback_count));
                        s_dropped_callback_count = 0;
                    }
                    break;
                }
                callback = std::move(s_callbacks.front().callback);
                s_callbacks.pop_front();
            }

            if (callback) {
                callback();
            }
        }
    }
}

}  // namespace

void Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_task != nullptr) {
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(WorkerTask,
                                                       kTaskName,
                                                       kTaskStackWords,
                                                       nullptr,
                                                       followup_task_config::kPriorityUiRefresh,
                                                       &s_task,
                                                       followup_task_config::kAppCore);
    if (created != pdPASS || s_task == nullptr) {
        ESP_LOGE(kTag, "Failed to start UI refresh dispatcher task");
        s_task = nullptr;
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

void Dispatch(std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    Init();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_callbacks.size() >= kMaxPendingCallbacks) {
            s_callbacks.pop_front();
            ++s_dropped_callback_count;
        }
        s_callbacks.push_back({
            .callback = std::move(callback),
            .keyed = false,
            .key = 0,
        });
    }

    xTaskNotifyGive(s_task);
}

void DispatchLatest(uint32_t key, std::function<void()> callback)
{
    if (!callback) {
        return;
    }

    Init();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_callbacks.erase(std::remove_if(s_callbacks.begin(),
                                         s_callbacks.end(),
                                         [key](const PendingCallback& queued) {
                                             return queued.keyed && queued.key == key;
                                         }),
                          s_callbacks.end());
        if (s_callbacks.size() >= kMaxPendingCallbacks) {
            s_callbacks.pop_front();
            ++s_dropped_callback_count;
        }
        s_callbacks.push_back({
            .callback = std::move(callback),
            .keyed = true,
            .key = key,
        });
    }

    xTaskNotifyGive(s_task);
}

}  // namespace ui_refresh_dispatcher
