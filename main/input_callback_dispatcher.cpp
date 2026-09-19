#include "input_callback_dispatcher.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <utility>

#include "esp_err.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr const char* kTag = "InputDispatch";
constexpr const char* kTaskName = "input_callbacks";
// Button handlers run here, including recording arm/start; 3072 overflowed on a BOOT press.
constexpr uint32_t kTaskStackWords = 4096;
constexpr size_t kMaxPendingCallbacks = 64;

struct PendingCallback {
    std::function<void()> callback = {};
    bool keyed = false;
    uint32_t key = 0;
};

std::mutex s_mutex;
std::deque<PendingCallback> s_callbacks = {};
TaskHandle_t s_task = nullptr;
size_t s_dropped_callback_count = 0;

// Statically allocated: by the time this task is created, Wi-Fi/lwIP have
// already claimed and released internal RAM unpredictably during their own
// async setup (DHCP, management frames, ...), so a dynamic allocation here
// competes with that churn for whatever scraps remain. A static reservation
// is carved out of internal RAM at link time, before any of that runtime
// contention exists, so it is unaffected by it.
StaticTask_t s_task_buffer;
StackType_t s_task_stack[kTaskStackWords];

void WorkerTask(void*) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (true) {
            std::function<void()> callback = {};
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                if (s_callbacks.empty()) {
                    if (s_dropped_callback_count > 0) {
                        ESP_LOGW(kTag,
                                 "Dropped %u stale input callbacks while saturated",
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

InputCallbackDispatcher& InputCallbackDispatcher::GetInstance() {
    static InputCallbackDispatcher instance;
    return instance;
}

void InputCallbackDispatcher::Initialize() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_task != nullptr) {
        return;
    }

    s_task = xTaskCreateStaticPinnedToCore(
        WorkerTask,
        kTaskName,
        kTaskStackWords,
        nullptr,
        followup_task_config::kPriorityTouch,
        s_task_stack,
        &s_task_buffer,
        followup_task_config::kAppCore);
    if (s_task == nullptr) {
        ESP_LOGE(kTag, "Failed to start input callback dispatcher task");
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
}

void InputCallbackDispatcher::Dispatch(std::function<void()> callback) {
    if (!callback) {
        return;
    }

    Initialize();

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

void InputCallbackDispatcher::DispatchLatest(uint32_t key, std::function<void()> callback) {
    if (!callback) {
        return;
    }

    Initialize();

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
