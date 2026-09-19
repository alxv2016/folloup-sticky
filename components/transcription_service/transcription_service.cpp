#include "transcription_service.h"

#include <memory>
#include <mutex>
#include <new>
#include <string>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gemini_service.h"

namespace transcription_service {
namespace {

constexpr const char* kTag = "TranscriptionSvc";
constexpr uint32_t kWorkerTaskStackWords = 8192;

struct TaskContext {
    recording_service::RecordedClipPtr clip = {};
};

std::mutex s_mutex;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
bool s_initialized = false;
bool s_request_in_flight = false;
int s_last_http_status = 0;
std::string s_last_status_message = {};
std::string s_last_error_code = {};
std::string s_last_error_message = {};
std::string s_last_transcript = {};

std::once_flag s_worker_once;
QueueHandle_t s_request_queue = nullptr;  // TaskContext*, owned by the receiver
StaticTask_t s_worker_task_buffer;

Snapshot BuildSnapshotLocked()
{
    Snapshot snapshot = {};
    snapshot.initialized = s_initialized;
    snapshot.provider_ready = gemini_service::GetSnapshot().runtime.ready;
    snapshot.request_in_flight = s_request_in_flight;
    snapshot.last_http_status = s_last_http_status;
    snapshot.last_status_message = s_last_status_message;
    snapshot.last_error_code = s_last_error_code;
    snapshot.last_error_message = s_last_error_message;
    snapshot.last_transcript = s_last_transcript;
    return snapshot;
}

void NotifyLocked()
{
    EventHandler handler = s_event_handler;
    void* context = s_event_context;
    if (handler == nullptr) {
        return;
    }
    const Event event = {
        .snapshot = BuildSnapshotLocked(),
    };
    handler(event, context);
}

// Runs the (blocking) Gemini audio transcription and publishes the result. The Gemini HTTP now
// lives in gemini_service::Transcribe; this service owns the async lifecycle + snapshot/events.
void RunTranscription(std::unique_ptr<TaskContext> context)
{
    if (!context || !context->clip || context->clip->empty()) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_http_status = 0;
        s_last_status_message = "Transcription failed";
        s_last_error_code = "empty_audio";
        s_last_error_message = "No recorded audio available";
        s_last_transcript.clear();
        NotifyLocked();
        return;
    }

    const gemini_service::TranscriptionResult result = gemini_service::Transcribe(*context->clip);

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_http_status = result.http_status;
        if (result.success) {
            s_last_status_message = "Transcript ready";
            s_last_error_code.clear();
            s_last_error_message.clear();
            s_last_transcript = result.transcript;
            ESP_LOGI(kTag,
                     "Gemini transcription succeeded: chars=%u wav_bytes=%u clip_ms=%u "
                     "upload_chunks=%u upload_elapsed_ms=%llu total_elapsed_ms=%llu",
                     static_cast<unsigned>(s_last_transcript.size()),
                     static_cast<unsigned>(result.wav_bytes),
                     static_cast<unsigned>(result.clip_duration_ms),
                     static_cast<unsigned>(result.upload_chunk_count),
                     static_cast<unsigned long long>(result.upload_elapsed_ms),
                     static_cast<unsigned long long>(result.total_elapsed_ms));
        } else {
            s_last_status_message = "Transcription failed";
            s_last_error_code = result.error_code;
            s_last_error_message = result.error_message;
            s_last_transcript.clear();
            ESP_LOGW(kTag,
                     "Gemini transcription failed: http=%d code=%s message=%s "
                     "internal_free=%u internal_largest=%u internal_min_ever=%u",
                     result.http_status, s_last_error_code.c_str(), s_last_error_message.c_str(),
                     static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(
                         heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)),
                     static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
        }
        NotifyLocked();
    }
}

// The worker has a PSRAM stack (see followup_task_config::CreatePsramStackTask): internal RAM
// cannot spare 8 KB per request. The completion path saves the transcript through
// recording_archive_service, which defers its NVS cache write for PSRAM-stack callers.
void WorkerTask(void*)
{
    while (true) {
        TaskContext* raw_context = nullptr;
        if (xQueueReceive(s_request_queue, &raw_context, portMAX_DELAY) == pdTRUE) {
            RunTranscription(std::unique_ptr<TaskContext>(raw_context));
        }
    }
}

// Creates the worker on first use. A failure is not retried: it means PSRAM or the queue is
// unavailable.
bool EnsureWorkerStarted()
{
    std::call_once(s_worker_once, [] {
        QueueHandle_t queue = xQueueCreate(1, sizeof(TaskContext*));
        if (queue == nullptr) {
            ESP_LOGE(kTag, "Transcription worker queue allocation failed");
            return;
        }
        s_request_queue = queue;
        if (followup_task_config::CreatePsramStackTask(WorkerTask,
                                                       "transcription",
                                                       kWorkerTaskStackWords,
                                                       nullptr,
                                                       followup_task_config::kPriorityGemini,
                                                       &s_worker_task_buffer,
                                                       followup_task_config::kSystemCore) ==
            nullptr) {
            ESP_LOGE(kTag, "Transcription worker task create failed");
            s_request_queue = nullptr;
            vQueueDelete(queue);
        }
    });
    return s_request_queue != nullptr;
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) {
        return ESP_OK;
    }

    s_initialized = true;
    s_request_in_flight = false;
    s_last_http_status = 0;
    s_last_status_message = gemini_service::GetSnapshot().runtime.ready
                                ? "Gemini ready for transcription"
                                : "Gemini transcription unavailable";
    s_last_error_code.clear();
    s_last_error_message.clear();
    s_last_transcript.clear();
    return ESP_OK;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

Snapshot GetSnapshot()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildSnapshotLocked();
}

bool BeginTranscription(recording_service::RecordedClipPtr clip)
{
    if (Init() != ESP_OK) {
        return false;
    }

    const gemini_service::Snapshot gemini_snapshot = gemini_service::GetSnapshot();
    const std::string api_key = gemini_service::GetEffectiveApiKey();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_request_in_flight) {
            s_last_status_message = "Transcription already running";
            s_last_error_code = "request_in_flight";
            s_last_error_message = "A transcription request is already running";
            NotifyLocked();
            return false;
        }
        if (!gemini_snapshot.runtime.ready || api_key.empty()) {
            s_last_http_status = 0;
            s_last_status_message = "Transcription unavailable";
            s_last_error_code = gemini_snapshot.settings.configured ? "provider_not_ready"
                                                                    : "not_configured";
            s_last_error_message = gemini_snapshot.settings.configured
                                       ? "Gemini is not ready yet"
                                       : "No Gemini API key configured";
            s_last_transcript.clear();
            NotifyLocked();
            return false;
        }
        if (!clip || clip->empty()) {
            s_last_http_status = 0;
            s_last_status_message = "Transcription unavailable";
            s_last_error_code = "empty_audio";
            s_last_error_message = "No recorded audio available";
            s_last_transcript.clear();
            NotifyLocked();
            return false;
        }

        s_request_in_flight = true;
        s_last_http_status = 0;
        s_last_status_message = "Transcribing recording";
        s_last_error_code.clear();
        s_last_error_message.clear();
        s_last_transcript.clear();
        NotifyLocked();
    }

    TaskContext* task_context = new (std::nothrow) TaskContext();
    if (task_context == nullptr) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_status_message = "Transcription unavailable";
        s_last_error_code = "task_context_alloc_failed";
        s_last_error_message = "Failed to allocate transcription task context";
        NotifyLocked();
        return false;
    }
    task_context->clip = std::move(clip);
    // Logged before hand-off: the worker owns and frees the context once it is queued.
    // Internal RAM is the scarce resource for TLS/lwIP on this board; logging it here and on
    // failure shows whether a transport error was memory starvation.
    ESP_LOGI(kTag, "Starting Gemini transcription: samples=%u internal_free=%u internal_largest=%u",
             static_cast<unsigned>(task_context->clip->sample_count()),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));

    // s_request_in_flight admits one request at a time, so the one-slot queue is free.
    if (!EnsureWorkerStarted() || xQueueSend(s_request_queue, &task_context, 0) != pdTRUE) {
        delete task_context;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_status_message = "Transcription unavailable";
        s_last_error_code = "task_start_failed";
        s_last_error_message = "Failed to queue transcription task";
        NotifyLocked();
        return false;
    }
    return true;
}

}  // namespace transcription_service
