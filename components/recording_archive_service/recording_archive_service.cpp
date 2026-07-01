#include "recording_archive_service.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "storage_service.h"

namespace recording_archive_service {
namespace {

constexpr const char* kTag = "RecordingArchive";
constexpr int64_t kMinValidEpoch = 1704067200;  // 2024-01-01 UTC
constexpr int kMetadataVersion = 1;

struct ArchiveMetadata {
    int version = kMetadataVersion;
    std::string recording_id = {};
    int64_t created_unix_seconds = 0;
    std::string created_local_date = {};
    bool time_valid = false;
    uint32_t duration_ms = 0;
    bool has_transcript = false;
    RecordingTag tag = RecordingTag::kNote;
    bool completed = false;
    bool follow_up = false;
    bool follow_up_completed = false;
};

std::mutex s_mutex;
Snapshot s_snapshot = {};
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;

struct WavHeader {
    char riff[4];
    uint32_t chunk_size;
    char wave[4];
    char fmt[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};

static_assert(sizeof(WavHeader) == 44, "WAV header must be 44 bytes");

// A recording's tag decides whether it belongs to the Todos list or the Notes
// list. This is the single source of truth for that split: routing to disk and
// the archive counts both derive from it, so a newly added tag can never fall
// through and be silently uncounted (Idea previously hit the counting switch's
// default and never surfaced under Notes despite being saved).
bool IsTodoRecordingTag(RecordingTag tag)
{
    return tag == RecordingTag::kTask;
}

bool IsNotesRecordingTag(RecordingTag tag)
{
    return !IsTodoRecordingTag(tag);
}

const char* ArchiveSubdirectory(RecordingTag tag)
{
    return IsTodoRecordingTag(tag) ? "todos" : "recordings";
}

std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == '/') {
        return left + right;
    }
    return left + "/" + right;
}

bool EnsureDirectoryExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    errno = 0;
    if (mkdir(path.c_str(), 0775) == 0 || errno == EEXIST) {
        return true;
    }

    ESP_LOGW(kTag,
             "Create directory failed: path=%s errno=%d (%s)",
             path.c_str(),
             errno,
             std::strerror(errno));
    return false;
}

void LogFileStat(const char* label, const std::string& path)
{
    struct stat st = {};
    if (stat(path.c_str(), &st) != 0) {
        ESP_LOGW(kTag, "%s stat failed: path=%s errno=%d", label, path.c_str(), errno);
        return;
    }

    ESP_LOGI(kTag,
             "%s ready: path=%s size=%llu",
             label,
             path.c_str(),
             static_cast<unsigned long long>(st.st_size));
}

std::string GenerateRecordingId()
{
    const int64_t now_us = esp_timer_get_time();
    const int64_t now_s = now_us / 1000000LL;
    const uint32_t micros_part = static_cast<uint32_t>(now_us % 1000000LL);

    char buffer[48] = {};
    std::snprintf(buffer,
                  sizeof(buffer),
                  "rec_%lld_%06u",
                  static_cast<long long>(now_s),
                  static_cast<unsigned>(micros_part));
    return buffer;
}

std::string FormatLocalDate(int64_t unix_seconds, bool* time_valid_out)
{
    const bool time_valid = unix_seconds >= kMinValidEpoch;
    if (time_valid_out != nullptr) {
        *time_valid_out = time_valid;
    }
    if (!time_valid) {
        return {};
    }

    const time_t raw_time = static_cast<time_t>(unix_seconds);
    struct tm local_time = {};
    if (localtime_r(&raw_time, &local_time) == nullptr) {
        return {};
    }

    char buffer[32] = {};
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_time) == 0) {
        return {};
    }
    return buffer;
}

WavHeader BuildWavHeader(const recording_service::RecordedClip& clip)
{
    const uint32_t data_size = static_cast<uint32_t>(clip.pcm16_byte_count());
    WavHeader header = {};
    std::memcpy(header.riff, "RIFF", sizeof(header.riff));
    header.chunk_size = static_cast<uint32_t>(36U + data_size);
    std::memcpy(header.wave, "WAVE", sizeof(header.wave));
    std::memcpy(header.fmt, "fmt ", sizeof(header.fmt));
    header.subchunk1_size = 16;
    header.audio_format = 1;
    header.num_channels = 1;
    header.sample_rate = clip.sample_rate_hz();
    header.byte_rate = clip.sample_rate_hz() * sizeof(int16_t);
    header.block_align = sizeof(int16_t);
    header.bits_per_sample = 16;
    std::memcpy(header.data, "data", sizeof(header.data));
    header.data_size = data_size;
    return header;
}

bool WriteFileBytes(const std::string& path, const void* data, size_t size)
{
    errno = 0;
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag,
                 "Open file for write failed: %s errno=%d (%s)",
                 path.c_str(),
                 errno,
                 std::strerror(errno));
        return false;
    }

    const bool ok = size == 0 || std::fwrite(data, 1, size, file) == size;
    const int write_errno = errno;
    std::fclose(file);
    if (!ok) {
        ESP_LOGW(kTag,
                 "Write file failed: %s errno=%d (%s)",
                 path.c_str(),
                 write_errno,
                 std::strerror(write_errno));
    }
    return ok;
}

bool WriteClipWav(const std::string& path, const recording_service::RecordedClip& clip)
{
    errno = 0;
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag,
                 "Open WAV failed: %s errno=%d (%s)",
                 path.c_str(),
                 errno,
                 std::strerror(errno));
        return false;
    }

    const WavHeader header = BuildWavHeader(clip);
    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    clip.ForEachChunk([&](const int16_t* samples, size_t sample_count) {
        if (!ok || samples == nullptr || sample_count == 0) {
            return;
        }
        ok = std::fwrite(samples, sizeof(int16_t), sample_count, file) == sample_count;
    });

    const int write_errno = errno;
    std::fclose(file);
    if (!ok) {
        ESP_LOGW(kTag,
                 "Write WAV failed: %s errno=%d (%s)",
                 path.c_str(),
                 write_errno,
                 std::strerror(write_errno));
    }
    return ok;
}

std::string SerializeMetadata(const ArchiveMetadata& metadata)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", metadata.version);
    cJSON_AddStringToObject(root, "recording_id", metadata.recording_id.c_str());
    cJSON_AddNumberToObject(root, "created_unix_seconds",
                            static_cast<double>(metadata.created_unix_seconds));
    cJSON_AddStringToObject(root, "created_local_date", metadata.created_local_date.c_str());
    cJSON_AddBoolToObject(root, "time_valid", metadata.time_valid);
    cJSON_AddNumberToObject(root, "duration_ms", metadata.duration_ms);
    cJSON_AddBoolToObject(root, "has_transcript", metadata.has_transcript);
    cJSON_AddStringToObject(root, "tag", TagName(metadata.tag));
    cJSON_AddBoolToObject(root, "completed", metadata.completed);
    cJSON_AddBoolToObject(root, "follow_up", metadata.follow_up);
    cJSON_AddBoolToObject(root, "follow_up_completed", metadata.follow_up_completed);

    char* raw = cJSON_PrintUnformatted(root);
    std::string json = raw != nullptr ? raw : "";
    if (raw != nullptr) {
        cJSON_free(raw);
    }
    cJSON_Delete(root);
    return json;
}

bool ParseMetadata(const std::string& json, ArchiveMetadata* metadata)
{
    if (metadata == nullptr) {
        return false;
    }

    cJSON* root = cJSON_ParseWithLength(json.c_str(), json.size());
    if (root == nullptr) {
        return false;
    }

    ArchiveMetadata parsed = {};
    cJSON* version = cJSON_GetObjectItemCaseSensitive(root, "version");
    if (cJSON_IsNumber(version)) {
        parsed.version = version->valueint;
    }

    cJSON* recording_id = cJSON_GetObjectItemCaseSensitive(root, "recording_id");
    if (cJSON_IsString(recording_id) && recording_id->valuestring != nullptr) {
        parsed.recording_id = recording_id->valuestring;
    }

    cJSON* created_unix_seconds = cJSON_GetObjectItemCaseSensitive(root, "created_unix_seconds");
    if (cJSON_IsNumber(created_unix_seconds)) {
        parsed.created_unix_seconds = static_cast<int64_t>(created_unix_seconds->valuedouble);
    }

    cJSON* created_local_date = cJSON_GetObjectItemCaseSensitive(root, "created_local_date");
    if (cJSON_IsString(created_local_date) && created_local_date->valuestring != nullptr) {
        parsed.created_local_date = created_local_date->valuestring;
    }

    cJSON* time_valid = cJSON_GetObjectItemCaseSensitive(root, "time_valid");
    parsed.time_valid = cJSON_IsTrue(time_valid);

    cJSON* duration_ms = cJSON_GetObjectItemCaseSensitive(root, "duration_ms");
    if (cJSON_IsNumber(duration_ms)) {
        parsed.duration_ms = static_cast<uint32_t>(duration_ms->valueint);
    }

    cJSON* has_transcript = cJSON_GetObjectItemCaseSensitive(root, "has_transcript");
    parsed.has_transcript = cJSON_IsTrue(has_transcript);

    cJSON* tag = cJSON_GetObjectItemCaseSensitive(root, "tag");
    if (cJSON_IsString(tag) && tag->valuestring != nullptr) {
        if (std::strcmp(tag->valuestring, "task") == 0) {
            parsed.tag = RecordingTag::kTask;
        } else if (std::strcmp(tag->valuestring, "idea") == 0) {
            parsed.tag = RecordingTag::kIdea;
        } else {
            parsed.tag = RecordingTag::kNote;
        }
    }

    parsed.completed = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "completed"));
    parsed.follow_up = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "follow_up"));
    parsed.follow_up_completed =
        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "follow_up_completed"));

    cJSON_Delete(root);
    *metadata = parsed;
    return true;
}

bool ReadTextFile(const std::string& path, std::string* text)
{
    if (text == nullptr) {
        return false;
    }

    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long size = std::ftell(file);
    if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }

    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = size == 0 ? 0 : std::fread(content.data(), 1, content.size(), file);
    std::fclose(file);
    if (read != content.size()) {
        return false;
    }

    *text = std::move(content);
    return true;
}

std::array<std::string, 2> CandidateBasePaths(const std::string& mount_point,
                                              const std::string& recording_id)
{
    return {
        JoinPath(JoinPath(mount_point, "recordings"), recording_id),
        JoinPath(JoinPath(mount_point, "todos"), recording_id),
    };
}

bool ResolveExistingBasePath(const std::string& mount_point,
                             const std::string& recording_id,
                             std::string* base_path)
{
    if (base_path == nullptr) {
        return false;
    }

    for (const std::string& candidate : CandidateBasePaths(mount_point, recording_id)) {
        struct stat st = {};
        if (stat((candidate + ".wav").c_str(), &st) == 0) {
            *base_path = candidate;
            return true;
        }
    }
    return false;
}

struct SaveClipContext {
    const recording_service::RecordedClip* clip = nullptr;
    SaveOptions options = {};
    SaveResult* result = nullptr;
};

esp_err_t SaveClipOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* save = static_cast<SaveClipContext*>(context);
    if (mount_point == nullptr || save == nullptr || save->clip == nullptr || save->result == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    SaveResult& result = *save->result;
    const std::string directory = JoinPath(mount_point, ArchiveSubdirectory(save->options.tag));
    if (!EnsureDirectoryExists(directory)) {
        result.error_code = "directory_create_failed";
        result.error_message = "Failed to prepare SD archive directory";
        return ESP_FAIL;
    }

    result.recording_id = GenerateRecordingId();
    const std::string base_path = JoinPath(directory, result.recording_id);
    result.recording_path = base_path + ".wav";
    result.metadata_path = base_path + ".json";

    ESP_LOGI(kTag,
             "Saving recording to SD: id=%s tag=%s wav_path=%s metadata_path=%s",
             result.recording_id.c_str(),
             TagName(save->options.tag),
             result.recording_path.c_str(),
             result.metadata_path.c_str());

    const int64_t now_s = static_cast<int64_t>(std::time(nullptr));
    ArchiveMetadata metadata = {};
    metadata.recording_id = result.recording_id;
    metadata.created_unix_seconds = now_s;
    metadata.created_local_date = FormatLocalDate(now_s, &metadata.time_valid);
    metadata.duration_ms = save->clip->duration_ms();
    metadata.has_transcript = false;
    metadata.tag = save->options.tag;

    result.clip_saved = WriteClipWav(result.recording_path, *save->clip);
    if (!result.clip_saved) {
        result.error_code = "clip_write_failed";
        result.error_message = "Failed to save WAV to SD";
        return ESP_FAIL;
    }

    const std::string metadata_json = SerializeMetadata(metadata);
    ESP_LOGI(kTag,
             "Writing metadata JSON: id=%s bytes=%u has_transcript=%d time_valid=%d",
             result.recording_id.c_str(),
             static_cast<unsigned>(metadata_json.size()),
             metadata.has_transcript ? 1 : 0,
             metadata.time_valid ? 1 : 0);
    result.metadata_saved = WriteFileBytes(result.metadata_path,
                                           metadata_json.data(),
                                           metadata_json.size());
    result.success = result.clip_saved && result.metadata_saved;
    result.status_message = result.success ? "Recording saved" : "Recording saved with metadata issue";
    if (!result.metadata_saved) {
        result.error_code = "metadata_write_failed";
        result.error_message = "Failed to save recording metadata";
        return ESP_FAIL;
    }

    ESP_LOGI(kTag,
             "Saved recording: id=%s tag=%s duration_ms=%lu path=%s",
             result.recording_id.c_str(),
             TagName(save->options.tag),
             static_cast<unsigned long>(metadata.duration_ms),
             result.recording_path.c_str());
    LogFileStat("WAV file", result.recording_path);
    LogFileStat("Metadata JSON", result.metadata_path);
    return ESP_OK;
}

struct SaveTranscriptContext {
    const char* recording_id = nullptr;
    const char* transcript = nullptr;
    SaveResult* result = nullptr;
};

esp_err_t SaveTranscriptOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* save = static_cast<SaveTranscriptContext*>(context);
    if (mount_point == nullptr || save == nullptr || save->recording_id == nullptr ||
        save->transcript == nullptr || save->result == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    SaveResult& result = *save->result;
    result.recording_id = save->recording_id;

    std::string base_path;
    if (!ResolveExistingBasePath(mount_point, save->recording_id, &base_path)) {
        result.error_code = "recording_not_found";
        result.error_message = "Saved recording was not found on SD";
        return ESP_ERR_NOT_FOUND;
    }

    result.recording_path = base_path + ".wav";
    result.transcript_path = base_path + ".txt";
    result.metadata_path = base_path + ".json";

    ESP_LOGI(kTag,
             "Saving transcript to SD: id=%s transcript_path=%s metadata_path=%s chars=%u",
             result.recording_id.c_str(),
             result.transcript_path.c_str(),
             result.metadata_path.c_str(),
             static_cast<unsigned>(std::strlen(save->transcript)));

    result.transcript_saved = WriteFileBytes(result.transcript_path,
                                             save->transcript,
                                             std::strlen(save->transcript));
    if (!result.transcript_saved) {
        result.error_code = "transcript_write_failed";
        result.error_message = "Failed to save transcript to SD";
        return ESP_FAIL;
    }

    ArchiveMetadata metadata = {};
    std::string metadata_json;
    if (ReadTextFile(result.metadata_path, &metadata_json) && ParseMetadata(metadata_json, &metadata)) {
        metadata.has_transcript = true;
        const std::string updated_json = SerializeMetadata(metadata);
        ESP_LOGI(kTag,
                 "Updating metadata JSON for transcript: id=%s bytes=%u",
                 result.recording_id.c_str(),
                 static_cast<unsigned>(updated_json.size()));
        result.metadata_saved = WriteFileBytes(result.metadata_path,
                                               updated_json.data(),
                                               updated_json.size());
    } else {
        result.metadata_saved = false;
    }

    result.success = result.transcript_saved;
    result.status_message = result.transcript_saved ? "Transcript ready" : "Transcript save failed";
    if (!result.metadata_saved) {
        result.error_code = "metadata_update_failed";
        result.error_message = "Transcript saved, but metadata update failed";
    }

    ESP_LOGI(kTag,
             "Saved transcript: id=%s transcript_path=%s metadata_saved=%d",
             result.recording_id.c_str(),
             result.transcript_path.c_str(),
             result.metadata_saved ? 1 : 0);
    LogFileStat("Transcript TXT", result.transcript_path);
    LogFileStat("Metadata JSON", result.metadata_path);
    if (result.metadata_saved) {
        ESP_LOGI(kTag,
                 "Transcript metadata update complete: id=%s has_transcript=1",
                 result.recording_id.c_str());
    } else {
        ESP_LOGW(kTag,
                 "Transcript metadata update incomplete: id=%s error=%s",
                 result.recording_id.c_str(),
                 result.error_code.empty() ? "<none>" : result.error_code.c_str());
    }
    return ESP_OK;
}

void NotifyHandler()
{
    EventHandler handler = nullptr;
    void* context = nullptr;
    Event event = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_event_handler;
        context = s_event_context;
        event.snapshot = s_snapshot;
    }
    if (handler != nullptr) {
        handler(event, context);
    }
}

void ScanDirectoryInto(const std::string& directory, Snapshot* snapshot)
{
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return;  // directory may not exist yet
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name.size() < 6 || name.compare(name.size() - 5, 5, ".json") != 0) {
            continue;
        }

        std::string json;
        ArchiveMetadata metadata = {};
        if (!ReadTextFile(JoinPath(directory, name), &json) || !ParseMetadata(json, &metadata)) {
            continue;
        }

        snapshot->recording_count++;
        if (metadata.follow_up) {
            snapshot->follow_up_recording_count++;
        }
        if (IsTodoRecordingTag(metadata.tag)) {
            snapshot->todo_recording_count++;
            if (metadata.completed) {
                snapshot->completed_todo_count++;
            } else {
                snapshot->incomplete_todo_count++;
            }
        } else {
            // Note + Idea both live under Notes.
            snapshot->notes_recording_count++;
        }
    }
    closedir(dir);
}

struct ScanContext {
    Snapshot* snapshot = nullptr;
};

esp_err_t ScanArchiveOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* scan = static_cast<ScanContext*>(context);
    if (mount_point == nullptr || scan == nullptr || scan->snapshot == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    ScanDirectoryInto(JoinPath(mount_point, "recordings"), scan->snapshot);
    ScanDirectoryInto(JoinPath(mount_point, "todos"), scan->snapshot);
    scan->snapshot->available = true;
    return ESP_OK;
}

struct MutateContext {
    const char* recording_id = nullptr;
    bool set_completed = false;
    bool completed = false;
    bool set_follow_up = false;
    bool follow_up = false;
    bool follow_up_completed = false;
    bool applied = false;
};

esp_err_t MutateMetadataOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* mutate = static_cast<MutateContext*>(context);
    if (mount_point == nullptr || mutate == nullptr || mutate->recording_id == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::string base_path;
    if (!ResolveExistingBasePath(mount_point, mutate->recording_id, &base_path)) {
        return ESP_ERR_NOT_FOUND;
    }

    const std::string metadata_path = base_path + ".json";
    std::string json;
    ArchiveMetadata metadata = {};
    if (!ReadTextFile(metadata_path, &json) || !ParseMetadata(json, &metadata)) {
        return ESP_FAIL;
    }

    if (mutate->set_completed) {
        metadata.completed = mutate->completed;
    }
    if (mutate->set_follow_up) {
        metadata.follow_up = mutate->follow_up;
        metadata.follow_up_completed = mutate->follow_up_completed;
    }

    const std::string updated = SerializeMetadata(metadata);
    mutate->applied = WriteFileBytes(metadata_path, updated.data(), updated.size());
    return mutate->applied ? ESP_OK : ESP_FAIL;
}

}  // namespace

SaveResult SaveClip(const recording_service::RecordedClip& clip, const SaveOptions& options)
{
    SaveResult result = {};
    if (clip.empty()) {
        result.error_code = "empty_audio";
        result.error_message = "Recording clip was empty";
        return result;
    }

    SaveClipContext context = {};
    context.clip = &clip;
    context.options = options;
    context.result = &result;
    const esp_err_t err = storage_service::RunWithMountedFilesystem(SaveClipOnMountedFilesystem,
                                                                    &context);
    if (err != ESP_OK && result.error_code.empty()) {
        result.error_code = "storage_error";
        result.error_message = esp_err_to_name(err);
    }
    if (result.success) {
        (void)Refresh();  // recompute archive counts + notify subscribers
    }
    return result;
}

SaveResult SaveTranscript(const std::string& recording_id, const std::string& transcript)
{
    SaveResult result = {};
    if (recording_id.empty()) {
        result.error_code = "recording_id_missing";
        result.error_message = "Recording ID was missing";
        return result;
    }
    if (transcript.empty()) {
        result.error_code = "empty_transcript";
        result.error_message = "Transcript text was empty";
        return result;
    }

    SaveTranscriptContext context = {};
    context.recording_id = recording_id.c_str();
    context.transcript = transcript.c_str();
    context.result = &result;
    const esp_err_t err =
        storage_service::RunWithMountedFilesystem(SaveTranscriptOnMountedFilesystem, &context);
    if (err != ESP_OK && result.error_code.empty()) {
        result.error_code = "storage_error";
        result.error_message = esp_err_to_name(err);
    }
    return result;
}

const char* TagName(RecordingTag tag)
{
    switch (tag) {
        case RecordingTag::kTask:
            return "task";
        case RecordingTag::kIdea:
            return "idea";
        case RecordingTag::kNote:
        default:
            return "note";
    }
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
    return s_snapshot;
}

bool Refresh()
{
    Snapshot scanned = {};
    scanned.initialized = true;
    ScanContext context = {.snapshot = &scanned};
    const esp_err_t err =
        storage_service::RunWithMountedFilesystem(ScanArchiveOnMountedFilesystem, &context);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot = scanned;
    }
    NotifyHandler();
    return err == ESP_OK;
}

void Init()
{
    (void)Refresh();
}

bool MarkRecordingCompleted(const std::string& recording_id, bool completed)
{
    if (recording_id.empty()) {
        return false;
    }
    MutateContext context = {};
    context.recording_id = recording_id.c_str();
    context.set_completed = true;
    context.completed = completed;
    (void)storage_service::RunWithMountedFilesystem(MutateMetadataOnMountedFilesystem, &context);
    if (context.applied) {
        (void)Refresh();
    }
    return context.applied;
}

bool MarkRecordingFollowUp(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed)
{
    if (recording_id.empty()) {
        return false;
    }
    MutateContext context = {};
    context.recording_id = recording_id.c_str();
    context.set_follow_up = true;
    context.follow_up = follow_up;
    context.follow_up_completed = follow_up_completed;
    (void)storage_service::RunWithMountedFilesystem(MutateMetadataOnMountedFilesystem, &context);
    if (context.applied) {
        (void)Refresh();
    }
    return context.applied;
}

}  // namespace recording_archive_service
