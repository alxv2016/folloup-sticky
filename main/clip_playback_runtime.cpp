#include "clip_playback_runtime.h"

#include <atomic>
#include <memory>
#include <utility>

#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "playback_service.h"

namespace clip_playback_runtime {
namespace {

constexpr const char* kTag = "ClipPlayback";
constexpr uint32_t kWorkerStackWords = 4096;

std::atomic<bool> s_worker_active{false};

void PlaybackWorker(void* arg)
{
    std::unique_ptr<std::string> path(static_cast<std::string*>(arg));
    if (path != nullptr && !path->empty()) {
        const esp_err_t err = playback_service::PlayFile(path->c_str());
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Playback failed for %s: %s", path->c_str(), esp_err_to_name(err));
        }
    }
    s_worker_active.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

}  // namespace

bool PlayFileAsync(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    bool expected = false;
    if (!s_worker_active.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        ESP_LOGI(kTag, "Playback already active; ignoring request for %s", path.c_str());
        return false;
    }

    auto* path_arg = new std::string(path);
    const BaseType_t created = xTaskCreatePinnedToCore(
        PlaybackWorker, "clip_play", kWorkerStackWords, path_arg,
        followup_task_config::kPriorityRecordCapture, nullptr, followup_task_config::kAppCore);
    if (created != pdPASS) {
        delete path_arg;
        s_worker_active.store(false, std::memory_order_release);
        ESP_LOGW(kTag, "Failed to start playback worker for %s", path.c_str());
        return false;
    }
    return true;
}

}  // namespace clip_playback_runtime
