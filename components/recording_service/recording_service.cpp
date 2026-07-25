#include "recording_service.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

#include "esp_log.h"
#include "esp_timer.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "audio_codec.h"
#include "waveshare_board.h"
#include "storage_service.h"

namespace recording_service {
namespace {

constexpr const char* kTag = "RecordingService";
constexpr uint32_t kPrerollMs = 1000;
constexpr uint32_t kMaxRecordingMs = 10000;
constexpr size_t kCaptureChunkSamples = 480;
constexpr size_t kClipChunkSamples = 4096;
constexpr uint32_t kCaptureTaskStackWords = 4096;

// The ES8311 codec captures at a single duplex rate; recordings and the Gemini
// uplink are 16 kHz, so no resampling is needed.
constexpr uint32_t kSampleRateHz = 16000;

// Owned by the board (waveshare_board::GetAudioCodec); the recording service only
// borrows it for input capture.
AudioCodec* s_codec = nullptr;

// Average-absolute level in percent, matching the previous mic service so the UI
// mic indicator behaves identically.
uint8_t CalculateInputLevelPercent(const int16_t* samples, size_t sample_count)
{
    if (samples == nullptr || sample_count == 0) {
        return 0;
    }
    uint64_t abs_sum = 0;
    for (size_t i = 0; i < sample_count; ++i) {
        const int32_t sample = samples[i];
        abs_sum += static_cast<uint32_t>(sample < 0 ? -sample : sample);
    }
    const uint32_t avg_abs = static_cast<uint32_t>(abs_sum / sample_count);
    return static_cast<uint8_t>(std::min<uint32_t>((avg_abs * 100U) / 32768U, 100U));
}

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

class PcmRingBuffer {
public:
    void Reset(size_t capacity_samples)
    {
        samples_.assign(capacity_samples, 0);
        capacity_ = capacity_samples;
        write_index_ = 0;
        size_ = 0;
    }

    void Clear()
    {
        write_index_ = 0;
        size_ = 0;
    }

    void Push(const int16_t* samples, size_t sample_count)
    {
        if (samples == nullptr || sample_count == 0 || capacity_ == 0) {
            return;
        }

        for (size_t i = 0; i < sample_count; ++i) {
            samples_[write_index_] = samples[i];
            write_index_ = (write_index_ + 1) % capacity_;
            if (size_ < capacity_) {
                ++size_;
            }
        }
    }

    void ForEachOrdered(const std::function<void(const int16_t*, size_t)>& visitor) const
    {
        if (!visitor || size_ == 0 || capacity_ == 0) {
            return;
        }

        const size_t start = (write_index_ + capacity_ - size_) % capacity_;
        const size_t first_count = std::min(size_, capacity_ - start);
        visitor(samples_.data() + start, first_count);
        const size_t remaining = size_ - first_count;
        if (remaining > 0) {
            visitor(samples_.data(), remaining);
        }
    }

private:
    size_t capacity_ = 0;
    size_t write_index_ = 0;
    size_t size_ = 0;
    PsramVector<int16_t> samples_;
};

class ClipBuffer {
public:
    void Clear()
    {
        chunks_.clear();
        sample_count_ = 0;
    }

    size_t Append(const int16_t* samples, size_t sample_count, size_t max_samples)
    {
        if (samples == nullptr || sample_count == 0 || sample_count_ >= max_samples) {
            return 0;
        }

        size_t appended = 0;
        while (appended < sample_count && sample_count_ < max_samples) {
            if (chunks_.empty() || chunks_.back().size() == kClipChunkSamples) {
                chunks_.emplace_back();
                chunks_.back().reserve(kClipChunkSamples);
            }

            auto& chunk = chunks_.back();
            const size_t chunk_space = kClipChunkSamples - chunk.size();
            const size_t total_space = max_samples - sample_count_;
            const size_t to_copy = std::min({sample_count - appended, chunk_space, total_space});
            chunk.insert(chunk.end(), samples + appended, samples + appended + to_copy);
            appended += to_copy;
            sample_count_ += to_copy;
        }
        return appended;
    }

    RecordedClipPtr Finalize(uint32_t sample_rate_hz)
    {
        RecordedClip::ChunkList chunks;
        chunks.reserve(chunks_.size());
        for (auto& chunk : chunks_) {
            chunks.push_back(std::move(chunk));
        }
        return std::make_shared<RecordedClip>(sample_rate_hz, sample_count_, std::move(chunks));
    }

    size_t sample_count() const { return sample_count_; }

private:
    std::vector<PsramVector<int16_t>> chunks_;
    size_t sample_count_ = 0;
};

std::mutex s_mutex;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
TaskHandle_t s_capture_task = nullptr;
// How long POWER must be held after arming before the mic shows its
// recording-preview state. Above the ~180ms tap / single-click ceiling
// (button_service kShortPressMs) so a tap never flashes the mic, and below the
// 300ms long-press (kLongPressMs) so the preview appears a little before
// recording actually engages.
constexpr int64_t kMicPreviewArmDelayUs = 220 * 1000;

bool s_initialized = false;
bool s_capture_armed = false;
bool s_recording = false;
// True once a hold has lasted past kMicPreviewArmDelayUs while armed (but before
// recording). Drives the mic indicator's "about to record" preview so it lights
// during the hold without flashing on quick taps.
bool s_capture_preview = false;
esp_timer_handle_t s_preview_timer = nullptr;
bool s_has_clip = false;
bool s_saving = false;
bool s_exporting = false;
uint8_t s_input_level_percent = 0;
StopReason s_stop_reason = StopReason::kNone;
PcmRingBuffer s_preroll_buffer;
ClipBuffer s_clip_buffer;
RecordedClipPtr s_last_clip;

void SetSaveExportActive(bool active);

class SaveExportGuard {
public:
    SaveExportGuard()
    {
        SetSaveExportActive(true);
    }

    ~SaveExportGuard()
    {
        SetSaveExportActive(false);
    }

    SaveExportGuard(const SaveExportGuard&) = delete;
    SaveExportGuard& operator=(const SaveExportGuard&) = delete;
};

size_t PrerollCapacitySamples()
{
    return static_cast<size_t>(
        (static_cast<uint64_t>(kSampleRateHz) * kPrerollMs) / 1000U);
}

size_t MaxRecordingSamples()
{
    return static_cast<size_t>(
        (static_cast<uint64_t>(kSampleRateHz) * kMaxRecordingMs) / 1000U);
}

State ResolveStateLocked()
{
    if (s_recording) {
        return State::kRecording;
    }
    if (s_capture_armed) {
        return State::kArmed;
    }
    if (s_has_clip) {
        return State::kClipReady;
    }
    return State::kIdle;
}

UiState BuildUiStateLocked()
{
    UiState state = {};
    state.initialized = s_initialized;
    state.armed = s_capture_armed;
    state.recording = s_recording;
    state.preview = s_capture_preview;
    state.has_clip = s_has_clip;
    state.saving = s_saving;
    state.exporting = s_exporting;
    state.recorded_samples = s_clip_buffer.sample_count();
    if (s_has_clip && s_last_clip) {
        state.recorded_samples = s_last_clip->sample_count();
    }
    state.duration_ms = static_cast<uint32_t>(
        (static_cast<uint64_t>(state.recorded_samples) * 1000U) /
        kSampleRateHz);
    state.max_recording_ms = kMaxRecordingMs;
    state.input_level_percent = s_input_level_percent;
    state.stop_reason = s_stop_reason;
    return state;
}

Event BuildEvent()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    Event event = {};
    event.state = ResolveStateLocked();
    event.ui_state = BuildUiStateLocked();
    return event;
}

bool PathUsesStorageMount(const char* path)
{
    if (path == nullptr) {
        return false;
    }

    const char* mount_point = storage_service::MountPoint();
    if (mount_point == nullptr) {
        return false;
    }

    const size_t mount_len = std::strlen(mount_point);
    return std::strncmp(path, mount_point, mount_len) == 0 &&
           (path[mount_len] == '\0' || path[mount_len] == '/');
}

WavHeader BuildWavHeader(const RecordedClip& clip);

esp_err_t WriteClipToPath(const RecordedClip& clip, const char* path)
{
    struct stat st = {};
    if (stat(path, &st) == 0) {
        unlink(path);
    }

    FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Open WAV file failed: %s", path);
        return ESP_FAIL;
    }

    const WavHeader header = BuildWavHeader(clip);
    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    if (ok) {
        clip.ForEachChunk([&](const int16_t* samples, size_t sample_count) {
            if (!ok || samples == nullptr || sample_count == 0) {
                return;
            }
            ok = std::fwrite(samples, sizeof(int16_t), sample_count, file) == sample_count;
        });
    }

    std::fclose(file);
    if (!ok) {
        ESP_LOGW(kTag, "Write WAV file failed: %s", path);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Saved WAV: path=%s samples=%u duration_ms=%u bytes=%u",
             path,
             static_cast<unsigned>(clip.sample_count()),
             static_cast<unsigned>(clip.duration_ms()),
             static_cast<unsigned>(clip.wav_byte_count()));
    return ESP_OK;
}

struct SaveClipContext {
    RecordedClipPtr clip;
    const char* path = nullptr;
};

esp_err_t SaveClipViaMountedFilesystem(const char*, void* context)
{
    auto* save = static_cast<SaveClipContext*>(context);
    if (save == nullptr || !save->clip || save->path == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return WriteClipToPath(*save->clip, save->path);
}

void Notify()
{
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        handler = s_event_handler;
        context = s_event_context;
    }

    if (handler != nullptr) {
        handler(BuildEvent(), context);
    }
}

void StopPreviewTimer()
{
    if (s_preview_timer != nullptr) {
        esp_timer_stop(s_preview_timer);
    }
}

// Fires kMicPreviewArmDelayUs after arming. If POWER is still held (armed, not
// yet recording), light the mic preview so it appears during the hold, a little
// before the long-press starts recording. A quick tap disarms before this fires,
// so the timer is stopped and the mic never flashes.
void ArmPreviewTimerCallback(void*)
{
    bool notify = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_capture_armed && !s_recording && !s_capture_preview) {
            s_capture_preview = true;
            notify = true;
        }
    }
    if (notify) {
        Notify();
    }
}

void SetSaveExportActive(bool active)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_saving = active;
        s_exporting = active;
    }
    ESP_LOGI(kTag, "Recording save/export %s", active ? "started" : "finished");
    Notify();
}

WavHeader BuildWavHeader(const RecordedClip& clip)
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

void CaptureTask(void*)
{
    std::vector<int16_t> samples(kCaptureChunkSamples, 0);

    while (true) {
        bool should_capture = false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            should_capture = s_initialized && (s_capture_armed || s_recording);
        }

        if (!should_capture || s_codec == nullptr) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        // The codec read blocks until the chunk is filled (~30 ms at 16 kHz) and
        // returns 0 when input is disabled or on error.
        const int samples_read = s_codec->ReadInputSamples(samples);
        if (samples_read <= 0) {
            continue;
        }
        const size_t read_count = static_cast<size_t>(samples_read);

        bool reached_max = false;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_input_level_percent = CalculateInputLevelPercent(samples.data(), read_count);

            if (s_capture_armed) {
                s_preroll_buffer.Push(samples.data(), read_count);
            }

            if (s_recording) {
                s_clip_buffer.Append(samples.data(), read_count, MaxRecordingSamples());
                if (s_clip_buffer.sample_count() >= MaxRecordingSamples()) {
                    s_recording = false;
                    s_capture_armed = false;
                    s_stop_reason = StopReason::kMaxDuration;
                    s_last_clip =
                        s_clip_buffer.Finalize(kSampleRateHz);
                    s_has_clip = s_last_clip && !s_last_clip->empty();
                    reached_max = true;
                }
            }
        }

        if (reached_max) {
            s_codec->EnableInput(false);
            Notify();
        }
    }
}

}  // namespace

RecordedClip::RecordedClip(uint32_t sample_rate_hz, size_t sample_count, ChunkList chunks)
    : sample_rate_hz_(sample_rate_hz), sample_count_(sample_count), chunks_(std::move(chunks))
{
}

uint32_t RecordedClip::duration_ms() const
{
    if (sample_rate_hz_ == 0) {
        return 0;
    }
    return static_cast<uint32_t>((static_cast<uint64_t>(sample_count_) * 1000U) /
                                 sample_rate_hz_);
}

void RecordedClip::ForEachChunk(
    const std::function<void(const int16_t*, size_t)>& visitor) const
{
    if (!visitor) {
        return;
    }
    for (const auto& chunk : chunks_) {
        if (!chunk.empty()) {
            visitor(chunk.data(), chunk.size());
        }
    }
}

esp_err_t Init()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_initialized) {
            return ESP_OK;
        }
    }

    s_codec = waveshare_board::GetAudioCodec();
    if (s_codec == nullptr) {
        ESP_LOGE(kTag, "Audio codec unavailable; recording disabled");
        return ESP_ERR_NOT_FOUND;
    }
    s_codec->EnableInput(false);

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_preroll_buffer.Reset(PrerollCapacitySamples());
        s_clip_buffer.Clear();
        s_last_clip.reset();
        s_has_clip = false;
        s_capture_armed = false;
        s_recording = false;
        s_saving = false;
        s_exporting = false;
        s_stop_reason = StopReason::kNone;
        s_capture_preview = false;
        s_initialized = true;
    }

    if (s_preview_timer == nullptr) {
        const esp_timer_create_args_t timer_args = {
            .callback = &ArmPreviewTimerCallback,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "mic_preview",
            .skip_unhandled_events = true,
        };
        const esp_err_t timer_err = esp_timer_create(&timer_args, &s_preview_timer);
        if (timer_err != ESP_OK) {
            s_preview_timer = nullptr;
            ESP_LOGW(kTag, "Mic preview timer create failed: %s", esp_err_to_name(timer_err));
        }
    }

    if (s_capture_task == nullptr) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            CaptureTask,
            "record_capture",
            kCaptureTaskStackWords,
            nullptr,
            followup_task_config::kPriorityRecordCapture,
            &s_capture_task,
            followup_task_config::kAppCore);
        if (created != pdPASS) {
            s_capture_task = nullptr;
            std::lock_guard<std::mutex> lock(s_mutex);
            s_initialized = false;
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(kTag,
             "Recording service initialized: sample_rate=%lu preroll=%lums max=%lums",
             static_cast<unsigned long>(kSampleRateHz),
             static_cast<unsigned long>(kPrerollMs),
             static_cast<unsigned long>(kMaxRecordingMs));
    Notify();
    return ESP_OK;
}

bool IsInitialized()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_initialized;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

UiState GetUiState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildUiStateLocked();
}

esp_err_t Arm()
{
    esp_err_t err = Init();
    if (err != ESP_OK) {
        return err;
    }

    s_codec->EnableInput(true);

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_armed = true;
        s_recording = false;
        s_capture_preview = false;
        s_saving = false;
        s_exporting = false;
        s_stop_reason = StopReason::kNone;
        s_input_level_percent = 0;
        s_preroll_buffer.Clear();
        s_clip_buffer.Clear();
        s_last_clip.reset();
        s_has_clip = false;
    }

    // Arm the mic-preview lead: if POWER is still held past the tap window, the
    // mic lights before recording engages (see kMicPreviewArmDelayUs).
    if (s_preview_timer != nullptr) {
        esp_timer_stop(s_preview_timer);
        (void)esp_timer_start_once(s_preview_timer, kMicPreviewArmDelayUs);
    }

    ESP_LOGI(kTag, "Recording armed");
    Notify();
    return ESP_OK;
}

esp_err_t Start(StartMode mode)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized || !s_capture_armed || s_recording) {
            return ESP_ERR_INVALID_STATE;
        }

        s_clip_buffer.Clear();
        if (mode == StartMode::kIncludePreroll) {
            s_preroll_buffer.ForEachOrdered([](const int16_t* samples, size_t sample_count) {
                s_clip_buffer.Append(samples, sample_count, MaxRecordingSamples());
            });
        }
        s_recording = true;
        s_capture_preview = false;
        s_stop_reason = StopReason::kNone;
    }

    StopPreviewTimer();
    ESP_LOGI(kTag, "Recording started: mode=%s",
             mode == StartMode::kIncludePreroll ? "include_preroll" : "fresh");
    Notify();
    return ESP_OK;
}

esp_err_t Finish()
{
    bool had_recording = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized || (!s_capture_armed && !s_recording)) {
            return ESP_ERR_INVALID_STATE;
        }

        had_recording = s_recording;
        s_recording = false;
        s_capture_armed = false;
        s_capture_preview = false;
        s_stop_reason = StopReason::kFinished;
        if (had_recording) {
            s_last_clip = s_clip_buffer.Finalize(kSampleRateHz);
            s_has_clip = s_last_clip && !s_last_clip->empty();
        }
    }

    StopPreviewTimer();
    if (s_codec != nullptr) {
        s_codec->EnableInput(false);
    }
    ESP_LOGI(kTag, "Recording finished: had_recording=%d", had_recording ? 1 : 0);
    Notify();
    return ESP_OK;
}

esp_err_t Cancel()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        s_capture_armed = false;
        s_recording = false;
        s_capture_preview = false;
        s_saving = false;
        s_exporting = false;
        s_stop_reason = StopReason::kCanceled;
        s_preroll_buffer.Clear();
        s_clip_buffer.Clear();
        s_last_clip.reset();
        s_has_clip = false;
    }

    StopPreviewTimer();
    if (s_codec != nullptr) {
        s_codec->EnableInput(false);
    }
    ESP_LOGI(kTag, "Recording canceled");
    Notify();
    return ESP_OK;
}

void DiscardClip()
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_last_clip.reset();
        s_has_clip = false;
        s_saving = false;
        s_exporting = false;
        s_capture_preview = false;
        if (!s_capture_armed && !s_recording) {
            s_stop_reason = StopReason::kNone;
        }
    }
    StopPreviewTimer();
    Notify();
}

RecordedClipPtr GetRecordedClip()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_last_clip;
}

esp_err_t SaveLastClipWav(const char* path)
{
    if (path == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    RecordedClipPtr clip = GetRecordedClip();
    if (!clip || clip->empty()) {
        return ESP_ERR_INVALID_STATE;
    }

    SaveExportGuard save_export_guard;
    if (PathUsesStorageMount(path)) {
        SaveClipContext context = {};
        context.clip = clip;
        context.path = path;
        return storage_service::RunWithMountedFilesystem(SaveClipViaMountedFilesystem, &context);
    }

    return WriteClipToPath(*clip, path);
}

esp_err_t RecordDebugClipToWav(const char* path, uint32_t lead_in_ms, uint32_t record_ms)
{
    esp_err_t err = Arm();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Debug clip arm failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(kTag, "Collecting preroll for %lums", static_cast<unsigned long>(lead_in_ms));
    vTaskDelay(pdMS_TO_TICKS(lead_in_ms));

    err = Start(StartMode::kIncludePreroll);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Debug clip start failed: %s", esp_err_to_name(err));
        Cancel();
        return err;
    }

    ESP_LOGI(kTag, "Recording debug clip for %lums", static_cast<unsigned long>(record_ms));
    vTaskDelay(pdMS_TO_TICKS(record_ms));

    err = Finish();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Debug clip finish failed: %s", esp_err_to_name(err));
        return err;
    }

    return SaveLastClipWav(path);
}

void LogDebugStatus()
{
    const UiState state = GetUiState();
    ESP_LOGI(kTag,
             "status: initialized=%d armed=%d recording=%d has_clip=%d saving=%d exporting=%d samples=%u duration_ms=%lu level=%u stop_reason=%u",
             state.initialized ? 1 : 0,
             state.armed ? 1 : 0,
             state.recording ? 1 : 0,
             state.has_clip ? 1 : 0,
             state.saving ? 1 : 0,
             state.exporting ? 1 : 0,
             static_cast<unsigned>(state.recorded_samples),
             static_cast<unsigned long>(state.duration_ms),
             static_cast<unsigned>(state.input_level_percent),
             static_cast<unsigned>(state.stop_reason));
}

}  // namespace recording_service
