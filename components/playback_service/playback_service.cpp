#include "playback_service.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <vector>

#include "audio_codec.h"
#include "esp_log.h"
#include "waveshare_board.h"

namespace playback_service {
namespace {

constexpr const char* kTag = "PlaybackService";
constexpr size_t kWavHeaderBytes = 44;
constexpr size_t kChunkSamples = 512;

std::atomic<bool> s_playing{false};
std::atomic<bool> s_stop_requested{false};

// Minimal validation of a canonical 44-byte PCM WAV header. Recordings are always
// written by this firmware (16 kHz mono 16-bit), so this only sanity-checks the
// RIFF/WAVE tags rather than parsing every field.
bool ValidateWavHeader(const uint8_t* header)
{
    return std::memcmp(header, "RIFF", 4) == 0 &&
           std::memcmp(header + 8, "WAVE", 4) == 0;
}

// Claims the playback slot and resolves the codec. Returns nullptr (without claiming) if
// playback is already running or the codec is missing; the caller owns clearing s_playing
// via PlayingGuard.
AudioCodec* BeginPlayback(const char* what)
{
    bool expected = false;
    if (!s_playing.compare_exchange_strong(expected, true)) {
        ESP_LOGW(kTag, "Playback already in progress; ignoring %s", what);
        return nullptr;
    }
    s_stop_requested.store(false, std::memory_order_relaxed);

    AudioCodec* codec = waveshare_board::GetAudioCodec();
    if (codec == nullptr) {
        ESP_LOGE(kTag, "Audio codec unavailable; cannot play %s", what);
        s_playing.store(false, std::memory_order_relaxed);
        return nullptr;
    }
    return codec;
}

// Clears the playback flag on every return path once BeginPlayback has succeeded.
struct PlayingGuard {
    ~PlayingGuard() { s_playing.store(false, std::memory_order_relaxed); }
};

}  // namespace

bool IsPlaying()
{
    return s_playing.load(std::memory_order_relaxed);
}

void Stop()
{
    if (s_playing.load(std::memory_order_relaxed)) {
        s_stop_requested.store(true, std::memory_order_relaxed);
    }
}

esp_err_t PlayFile(const char* path)
{
    if (path == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    AudioCodec* codec = BeginPlayback(path);
    if (codec == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    PlayingGuard playing_guard;

    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGW(kTag, "Failed to open clip for playback: %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t header[kWavHeaderBytes] = {};
    if (fread(header, 1, sizeof(header), file) != sizeof(header) ||
        !ValidateWavHeader(header)) {
        ESP_LOGW(kTag, "Not a valid WAV clip: %s", path);
        fclose(file);
        return ESP_ERR_INVALID_ARG;
    }

    // The board keeps the codec output (and PA) enabled for its lifetime, so we
    // only stream data here — never toggle EnableOutput, or system sound cues that
    // share the output would be cut off.
    ESP_LOGI(kTag, "Playback started: %s", path);

    std::vector<int16_t> chunk(kChunkSamples, 0);
    esp_err_t result = ESP_OK;
    size_t total_samples = 0;
    while (!s_stop_requested.load(std::memory_order_relaxed)) {
        const size_t samples_read =
            fread(chunk.data(), sizeof(int16_t), chunk.size(), file);
        if (samples_read == 0) {
            break;  // End of clip.
        }
        if (!codec->OutputData(chunk.data(), samples_read)) {
            ESP_LOGW(kTag, "Codec output failed during playback of %s", path);
            result = ESP_FAIL;
            break;
        }
        total_samples += samples_read;
    }

    fclose(file);
    ESP_LOGI(kTag, "Playback finished: %s samples=%u stopped=%d",
             path, static_cast<unsigned>(total_samples),
             s_stop_requested.load(std::memory_order_relaxed) ? 1 : 0);
    return result;
}

esp_err_t PlayClip(const recording_service::RecordedClipPtr& clip)
{
    if (clip == nullptr || clip->empty()) {
        return ESP_ERR_INVALID_ARG;
    }

    AudioCodec* codec = BeginPlayback("in-memory clip");
    if (codec == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    PlayingGuard playing_guard;

    ESP_LOGI(kTag, "Clip playback started: %u samples (%u ms)",
             static_cast<unsigned>(clip->sample_count()),
             static_cast<unsigned>(clip->duration_ms()));

    // ForEachChunk has no way to break out, so once stopped or failed the remaining
    // chunks fall through as no-ops rather than being written to the codec.
    esp_err_t result = ESP_OK;
    size_t total_samples = 0;
    clip->ForEachChunk([&](const int16_t* samples, size_t count) {
        if (result != ESP_OK || samples == nullptr) {
            return;
        }
        // Match PlayFile's write size rather than handing the codec whole capture
        // chunks, whose length is set by the recorder, not by what the codec wants.
        for (size_t offset = 0; offset < count; offset += kChunkSamples) {
            if (s_stop_requested.load(std::memory_order_relaxed)) {
                return;
            }
            const size_t batch = std::min(kChunkSamples, count - offset);
            if (!codec->OutputData(samples + offset, batch)) {
                ESP_LOGW(kTag, "Codec output failed during clip playback");
                result = ESP_FAIL;
                return;
            }
            total_samples += batch;
        }
    });

    ESP_LOGI(kTag, "Clip playback finished: samples=%u stopped=%d",
             static_cast<unsigned>(total_samples),
             s_stop_requested.load(std::memory_order_relaxed) ? 1 : 0);
    return result;
}

}  // namespace playback_service
