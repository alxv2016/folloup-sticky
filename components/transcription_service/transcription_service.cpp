#include "transcription_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <strings.h>
#include <string>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gemini_service.h"

namespace transcription_service {
namespace {

constexpr const char* kTag = "TranscriptionSvc";
constexpr const char* kGeminiApiBaseUrl = "https://generativelanguage.googleapis.com/v1beta/";
constexpr const char* kUploadUrl = "https://generativelanguage.googleapis.com/upload/v1beta/files";
constexpr const char* kTranscriptPrompt =
    "Generate a verbatim transcript of the speech in this audio. Respond with transcript text "
    "only. Do not add commentary or formatting.";
constexpr const char* kAudioMimeType = "audio/wav";
constexpr int kHttpTimeoutMs = 30000;
constexpr uint32_t kWorkerTaskStackWords = 8192;
constexpr size_t kHttpUploadChunkSamples = 2048;

struct HttpResponse {
    int status_code = 0;
    std::string body = {};
    std::string error_code = {};
    std::string error_message = {};
    std::string upload_url = {};
};

struct TaskContext {
    std::string api_key = {};
    std::string model_name = {};
    recording_service::RecordedClipPtr clip = {};
};

struct Result {
    bool success = false;
    int http_status = 0;
    std::string transcript = {};
    std::string error_code = {};
    std::string error_message = {};
    uint32_t clip_duration_ms = 0;
    size_t wav_bytes = 0;
    uint32_t upload_chunk_count = 0;
    uint64_t upload_elapsed_ms = 0;
    uint64_t total_elapsed_ms = 0;
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

std::string TrimCopy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string TrimForLog(std::string value, size_t max_len = 96)
{
    value = TrimCopy(std::move(value));
    if (value.size() <= max_len) {
        return value;
    }
    if (max_len <= 3) {
        return value.substr(0, max_len);
    }
    return value.substr(0, max_len - 3) + "...";
}

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

esp_err_t HttpEventHandler(esp_http_client_event_t* event)
{
    if (event == nullptr) {
        return ESP_FAIL;
    }

    auto* response = static_cast<HttpResponse*>(event->user_data);
    if (event->event_id == HTTP_EVENT_ON_DATA && response != nullptr && event->data != nullptr &&
        event->data_len > 0) {
        response->body.append(static_cast<const char*>(event->data),
                              static_cast<size_t>(event->data_len));
    } else if (event->event_id == HTTP_EVENT_ON_HEADER && response != nullptr &&
               event->header_key != nullptr && event->header_value != nullptr &&
               strcasecmp(event->header_key, "x-goog-upload-url") == 0) {
        response->upload_url = event->header_value;
    }
    return ESP_OK;
}

std::string JsonStringField(cJSON* root, const char* key)
{
    if (root == nullptr || key == nullptr) {
        return {};
    }
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) {
        return {};
    }
    return item->valuestring;
}

std::string JsonNestedStringField(cJSON* root, const char* first, const char* second)
{
    if (root == nullptr) {
        return {};
    }
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, first);
    if (!cJSON_IsObject(item)) {
        return {};
    }
    return JsonStringField(item, second);
}

void PopulateHttpError(cJSON* root,
                       const HttpResponse& response,
                       std::string* error_code,
                       std::string* error_message)
{
    if (error_code == nullptr || error_message == nullptr) {
        return;
    }

    *error_code = "http_error";
    error_message->clear();
    if (root != nullptr) {
        cJSON* error = cJSON_GetObjectItemCaseSensitive(root, "error");
        if (cJSON_IsObject(error)) {
            const std::string status = JsonStringField(error, "status");
            const std::string message = JsonStringField(error, "message");
            if (!status.empty()) {
                *error_code = status;
            }
            if (!message.empty()) {
                *error_message = message;
            }
        }
    }
    if (error_message->empty()) {
        *error_message = response.body.empty() ? "Gemini request failed" : response.body;
    }
    *error_message = TrimForLog(std::move(*error_message));
}

std::string BuildTranscriptRequestJson(const std::string& file_uri)
{
    cJSON* root = cJSON_CreateObject();
    cJSON* contents = cJSON_AddArrayToObject(root, "contents");
    cJSON* content = cJSON_CreateObject();
    cJSON_AddItemToArray(contents, content);
    cJSON* parts = cJSON_AddArrayToObject(content, "parts");

    cJSON* prompt_part = cJSON_CreateObject();
    cJSON_AddStringToObject(prompt_part, "text", kTranscriptPrompt);
    cJSON_AddItemToArray(parts, prompt_part);

    cJSON* audio_part = cJSON_CreateObject();
    cJSON* file_data = cJSON_AddObjectToObject(audio_part, "fileData");
    cJSON_AddStringToObject(file_data, "mimeType", kAudioMimeType);
    cJSON_AddStringToObject(file_data, "fileUri", file_uri.c_str());
    cJSON_AddItemToArray(parts, audio_part);

    cJSON* generation_config = cJSON_AddObjectToObject(root, "generationConfig");
    cJSON_AddNumberToObject(generation_config, "temperature", 0);

    char* raw = cJSON_PrintUnformatted(root);
    std::string json = raw != nullptr ? raw : "";
    if (raw != nullptr) {
        cJSON_free(raw);
    }
    cJSON_Delete(root);
    return json;
}

HttpResponse PerformUploadStart(const std::string& api_key, size_t num_bytes)
{
    HttpResponse response = {};

    esp_http_client_config_t config = {};
    config.url = kUploadUrl;
    config.method = HTTP_METHOD_POST;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = kHttpTimeoutMs;
    config.event_handler = &HttpEventHandler;
    config.user_data = &response;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        response.error_code = "http_client_init_failed";
        response.error_message = "Failed to initialize Gemini upload client";
        return response;
    }

    char content_length[32] = {};
    std::snprintf(content_length, sizeof(content_length), "%u", static_cast<unsigned>(num_bytes));

    static constexpr const char* metadata = "{\"file\":{\"display_name\":\"STICKY_NOTE\"}}";
    esp_http_client_set_header(client, "x-goog-api-key", api_key.c_str());
    esp_http_client_set_header(client, "X-Goog-Upload-Protocol", "resumable");
    esp_http_client_set_header(client, "X-Goog-Upload-Command", "start");
    esp_http_client_set_header(client, "X-Goog-Upload-Header-Content-Length", content_length);
    esp_http_client_set_header(client, "X-Goog-Upload-Header-Content-Type", kAudioMimeType);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, metadata, std::strlen(metadata));

    const esp_err_t err = esp_http_client_perform(client);
    response.status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        response.error_code = "transport_error";
        response.error_message = esp_err_to_name(err);
    }
    return response;
}

void AppendLe16(uint16_t value, std::array<uint8_t, 44>* out, size_t* offset)
{
    if (out == nullptr || offset == nullptr || *offset + 2U > out->size()) {
        return;
    }
    (*out)[(*offset)++] = static_cast<uint8_t>(value & 0xFF);
    (*out)[(*offset)++] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void AppendLe32(uint32_t value, std::array<uint8_t, 44>* out, size_t* offset)
{
    if (out == nullptr || offset == nullptr || *offset + 4U > out->size()) {
        return;
    }
    (*out)[(*offset)++] = static_cast<uint8_t>(value & 0xFF);
    (*out)[(*offset)++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    (*out)[(*offset)++] = static_cast<uint8_t>((value >> 16) & 0xFF);
    (*out)[(*offset)++] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

std::array<uint8_t, 44> BuildWavHeaderPcm16Mono(size_t sample_count, uint32_t sample_rate_hz)
{
    constexpr uint16_t kChannels = 1;
    constexpr uint16_t kBitsPerSample = 16;
    constexpr uint16_t kBlockAlign = kChannels * (kBitsPerSample / 8U);
    const uint32_t data_bytes = static_cast<uint32_t>(sample_count * sizeof(int16_t));
    const uint32_t byte_rate = sample_rate_hz * kBlockAlign;

    std::array<uint8_t, 44> header = {};
    size_t offset = 0;
    header[offset++] = 'R';
    header[offset++] = 'I';
    header[offset++] = 'F';
    header[offset++] = 'F';
    AppendLe32(36U + data_bytes, &header, &offset);
    header[offset++] = 'W';
    header[offset++] = 'A';
    header[offset++] = 'V';
    header[offset++] = 'E';
    header[offset++] = 'f';
    header[offset++] = 'm';
    header[offset++] = 't';
    header[offset++] = ' ';
    AppendLe32(16U, &header, &offset);
    AppendLe16(1U, &header, &offset);
    AppendLe16(kChannels, &header, &offset);
    AppendLe32(sample_rate_hz, &header, &offset);
    AppendLe32(byte_rate, &header, &offset);
    AppendLe16(kBlockAlign, &header, &offset);
    AppendLe16(kBitsPerSample, &header, &offset);
    header[offset++] = 'd';
    header[offset++] = 'a';
    header[offset++] = 't';
    header[offset++] = 'a';
    AppendLe32(data_bytes, &header, &offset);
    return header;
}

bool ReadHttpResponseBody(esp_http_client_handle_t client, HttpResponse* response)
{
    if (client == nullptr || response == nullptr) {
        return false;
    }

    std::array<char, 512> buffer = {};
    while (true) {
        const int read = esp_http_client_read(client, buffer.data(), buffer.size());
        if (read < 0) {
            response->error_code = "transport_error";
            response->error_message = "Failed reading HTTP response body";
            return false;
        }
        if (read == 0) {
            break;
        }
        response->body.append(buffer.data(), static_cast<size_t>(read));
    }
    return true;
}

HttpResponse PerformUploadFinalizePcmWav(const std::string& upload_url,
                                         const recording_service::RecordedClip& clip)
{
    HttpResponse response = {};

    esp_http_client_config_t config = {};
    config.url = upload_url.c_str();
    config.method = HTTP_METHOD_POST;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = kHttpTimeoutMs;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        response.error_code = "http_client_init_failed";
        response.error_message = "Failed to initialize Gemini upload finalize client";
        return response;
    }

    const size_t total_bytes = clip.wav_byte_count();
    char content_length[32] = {};
    std::snprintf(content_length, sizeof(content_length), "%u", static_cast<unsigned>(total_bytes));
    esp_http_client_set_header(client, "Content-Length", content_length);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "X-Goog-Upload-Offset", "0");
    esp_http_client_set_header(client, "X-Goog-Upload-Command", "upload, finalize");

    esp_err_t err = esp_http_client_open(client, total_bytes);
    if (err != ESP_OK) {
        response.error_code = "transport_error";
        response.error_message = esp_err_to_name(err);
        esp_http_client_cleanup(client);
        return response;
    }

    const std::array<uint8_t, 44> header =
        BuildWavHeaderPcm16Mono(clip.sample_count(), clip.sample_rate_hz());
    const int header_written = esp_http_client_write(client,
                                                     reinterpret_cast<const char*>(header.data()),
                                                     static_cast<int>(header.size()));
    if (header_written != static_cast<int>(header.size())) {
        response.error_code = "transport_error";
        response.error_message = "Failed writing WAV header";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return response;
    }

    bool write_failed = false;
    clip.ForEachChunk([&](const int16_t* chunk_data, size_t chunk_size) {
        if (write_failed || chunk_data == nullptr || chunk_size == 0) {
            return;
        }
        const int bytes_to_write = static_cast<int>(chunk_size * sizeof(int16_t));
        const int written = esp_http_client_write(
            client, reinterpret_cast<const char*>(chunk_data), bytes_to_write);
        if (written != bytes_to_write) {
            write_failed = true;
        }
    });
    if (write_failed) {
        response.error_code = "transport_error";
        response.error_message = "Failed streaming Gemini audio upload";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return response;
    }

    const int response_length = esp_http_client_fetch_headers(client);
    response.status_code = esp_http_client_get_status_code(client);
    if (response.status_code <= 0 && response_length < 0) {
        response.error_code = "transport_error";
        response.error_message = "Failed fetching Gemini upload response headers";
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return response;
    }
    ReadHttpResponseBody(client, &response);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return response;
}

HttpResponse PerformGenerateContent(const std::string& api_key,
                                    const std::string& model_name,
                                    const std::string& request_json)
{
    HttpResponse response = {};
    std::string url = kGeminiApiBaseUrl;
    url += model_name;
    url += ":generateContent";

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = kHttpTimeoutMs;
    config.event_handler = &HttpEventHandler;
    config.user_data = &response;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        response.error_code = "http_client_init_failed";
        response.error_message = "Failed to initialize Gemini HTTP client";
        return response;
    }

    esp_http_client_set_header(client, "x-goog-api-key", api_key.c_str());
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");
    esp_http_client_set_post_field(client, request_json.c_str(), request_json.size());

    const esp_err_t err = esp_http_client_perform(client);
    response.status_code = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        response.error_code = "transport_error";
        response.error_message = esp_err_to_name(err);
    }
    return response;
}

std::string ExtractTranscriptText(cJSON* root)
{
    if (root == nullptr) {
        return {};
    }

    cJSON* candidates = cJSON_GetObjectItemCaseSensitive(root, "candidates");
    if (!cJSON_IsArray(candidates)) {
        return {};
    }
    cJSON* first_candidate = cJSON_GetArrayItem(candidates, 0);
    if (!cJSON_IsObject(first_candidate)) {
        return {};
    }
    cJSON* content = cJSON_GetObjectItemCaseSensitive(first_candidate, "content");
    if (!cJSON_IsObject(content)) {
        return {};
    }
    cJSON* parts = cJSON_GetObjectItemCaseSensitive(content, "parts");
    if (!cJSON_IsArray(parts)) {
        return {};
    }

    std::string transcript;
    cJSON* part = nullptr;
    cJSON_ArrayForEach(part, parts) {
        const std::string text = JsonStringField(part, "text");
        if (text.empty()) {
            continue;
        }
        if (!transcript.empty()) {
            transcript += "\n";
        }
        transcript += text;
    }
    return TrimCopy(std::move(transcript));
}

uint32_t ResolveUploadChunkCount(const recording_service::RecordedClip& clip)
{
    return static_cast<uint32_t>((clip.sample_count() + kHttpUploadChunkSamples - 1U) /
                                 kHttpUploadChunkSamples);
}

Result PerformTranscription(const std::string& api_key,
                            const std::string& model_name,
                            const recording_service::RecordedClip& clip)
{
    Result result = {};
    result.clip_duration_ms = clip.duration_ms();
    result.wav_bytes = clip.wav_byte_count();
    result.upload_chunk_count = ResolveUploadChunkCount(clip);

    const int64_t task_started_us = esp_timer_get_time();

    HttpResponse upload_start = PerformUploadStart(api_key, clip.wav_byte_count());
    if (!upload_start.error_code.empty() || upload_start.upload_url.empty() ||
        upload_start.status_code < 200 || upload_start.status_code >= 300) {
        result.http_status = upload_start.status_code;
        result.error_code = upload_start.error_code.empty() ? "upload_start_failed"
                                                            : upload_start.error_code;
        result.error_message = TrimForLog(!upload_start.error_message.empty()
                                              ? upload_start.error_message
                                              : (!upload_start.body.empty()
                                                     ? upload_start.body
                                                     : "Failed to start Gemini file upload"));
        return result;
    }

    const int64_t upload_started_us = esp_timer_get_time();
    HttpResponse upload_finalize = PerformUploadFinalizePcmWav(upload_start.upload_url, clip);
    result.http_status = upload_finalize.status_code;
    result.upload_elapsed_ms =
        static_cast<uint64_t>((esp_timer_get_time() - upload_started_us) / 1000ULL);
    if (!upload_finalize.error_code.empty() || upload_finalize.status_code < 200 ||
        upload_finalize.status_code >= 300) {
        result.error_code = upload_finalize.error_code.empty() ? "upload_finalize_failed"
                                                               : upload_finalize.error_code;
        result.error_message = TrimForLog(!upload_finalize.error_message.empty()
                                              ? upload_finalize.error_message
                                              : (!upload_finalize.body.empty()
                                                     ? upload_finalize.body
                                                     : "Failed to upload Gemini audio file"));
        return result;
    }

    cJSON* file_root =
        cJSON_ParseWithLength(upload_finalize.body.c_str(), upload_finalize.body.size());
    const std::string file_uri = JsonNestedStringField(file_root, "file", "uri");
    if (file_root != nullptr) {
        cJSON_Delete(file_root);
    }
    if (file_uri.empty()) {
        result.error_code = "file_uri_missing";
        result.error_message = "Gemini upload did not return a file URI";
        return result;
    }

    const std::string request_json = BuildTranscriptRequestJson(file_uri);
    HttpResponse http = PerformGenerateContent(api_key, model_name, request_json);
    result.http_status = http.status_code;
    result.total_elapsed_ms =
        static_cast<uint64_t>((esp_timer_get_time() - task_started_us) / 1000ULL);

    if (!http.error_code.empty()) {
        result.error_code = http.error_code;
        result.error_message = TrimForLog(http.error_message);
        return result;
    }

    cJSON* root = cJSON_ParseWithLength(http.body.c_str(), http.body.size());
    if (http.status_code >= 200 && http.status_code < 300) {
        result.transcript = ExtractTranscriptText(root);
        result.success = !result.transcript.empty();
        if (!result.success) {
            result.error_code = "empty_transcript";
            result.error_message = "Gemini returned no transcript text";
        }
        if (root != nullptr) {
            cJSON_Delete(root);
        }
        return result;
    }

    PopulateHttpError(root, http, &result.error_code, &result.error_message);
    if (root != nullptr) {
        cJSON_Delete(root);
    }
    return result;
}

void WorkerTask(void* raw_context)
{
    std::unique_ptr<TaskContext> context(static_cast<TaskContext*>(raw_context));
    if (!context || !context->clip || context->clip->empty()) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_http_status = 0;
        s_last_status_message = "Transcription failed";
        s_last_error_code = "empty_audio";
        s_last_error_message = "No recorded audio available";
        s_last_transcript.clear();
        NotifyLocked();
        vTaskDelete(nullptr);
        return;
    }

    const Result result = PerformTranscription(context->api_key, context->model_name, *context->clip);

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
                     "Gemini transcription succeeded: chars=%u wav_bytes=%u clip_ms=%u upload_chunks=%u upload_elapsed_ms=%llu total_elapsed_ms=%llu",
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
                     "Gemini transcription failed: http=%d code=%s message=%s",
                     result.http_status,
                     s_last_error_code.c_str(),
                     s_last_error_message.c_str());
        }
        NotifyLocked();
    }

    vTaskDelete(nullptr);
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
    const std::string model_name = gemini_service::GetEffectiveModelName();

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
    task_context->api_key = api_key;
    task_context->model_name = model_name;
    task_context->clip = std::move(clip);

    TaskHandle_t task = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        WorkerTask,
        "transcription",
        kWorkerTaskStackWords,
        task_context,
        followup_task_config::kPriorityGemini,
        &task,
        followup_task_config::kSystemCore);
    if (created != pdPASS) {
        delete task_context;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_request_in_flight = false;
        s_last_status_message = "Transcription unavailable";
        s_last_error_code = "task_start_failed";
        s_last_error_message = "Failed to queue transcription task";
        NotifyLocked();
        return false;
    }

    ESP_LOGI(kTag,
             "Starting Gemini transcription: samples=%u model=%s",
             static_cast<unsigned>(task_context->clip->sample_count()),
             model_name.c_str());
    return true;
}

}  // namespace transcription_service
