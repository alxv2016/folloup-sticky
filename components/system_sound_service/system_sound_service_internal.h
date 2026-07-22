#ifndef SYSTEM_SOUND_SERVICE_INTERNAL_H_
#define SYSTEM_SOUND_SERVICE_INTERNAL_H_

#include "system_sound_service.h"

#include "psram_allocator.h"

#include <freertos/task.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace system_sound_service_internal {

inline constexpr const char* kTag = "SystemSoundService";
// Matches the ES8311 codec's fixed 16 kHz duplex rate; the decoder resamples each
// cue to this on load.
inline constexpr uint32_t kOutputSampleRateHz = 16000;
inline constexpr size_t kDecoderInputChunkBytes = 768;
inline constexpr size_t kInitialDecoderOutputBytes = 4096;
inline constexpr size_t kPlaybackChunkSamples = 480;
inline constexpr size_t kSoundCueCount = static_cast<size_t>(SoundCue::kVolume) + 1U;
inline constexpr UBaseType_t kPlaybackTaskPriority = 4;
inline constexpr BaseType_t kPlaybackTaskCore = 0;

// A cue's embedded MP3 payload (linked into the app via EMBED_FILES).
struct EmbeddedMp3 {
    const uint8_t* data = nullptr;
    size_t size = 0;
};

const char* CueName(SoundCue cue);
int64_t CueDebounceWindowUs(SoundCue cue);
EmbeddedMp3 CueMp3(SoundCue cue);
size_t CueIndex(SoundCue cue);

struct CachedCuePcm {
    bool ready = false;
    PsramVector<int16_t> samples;
};

class SystemSoundServiceImpl {
public:
    static SystemSoundServiceImpl& GetInstance();

    void Initialize(AudioCodec* codec);
    void PlayCue(SoundCue cue);
    void PlayCue(SoundCue cue, std::function<void(SoundCuePlaybackResult)> on_complete);

private:
    void DispatchCompletion(std::function<void(SoundCuePlaybackResult)> callback,
                            SoundCuePlaybackResult result);
    void ReplacePendingCompletion(uint32_t generation,
                                  std::function<void(SoundCuePlaybackResult)> callback);
    void CompletePendingCompletion(uint32_t generation, SoundCuePlaybackResult result);
    bool ShouldInterrupt(uint32_t generation) const;
    const PsramVector<int16_t>* GetOrDecodeCuePcm(SoundCue cue, uint32_t generation);
    void PreloadInteractionCues();
    void PlaybackTask();
    void PlayCueNow(SoundCue cue, uint32_t generation);
    bool DecodeMp3ToOutputPcm(const std::vector<uint8_t>& encoded,
                              PsramVector<int16_t>* output_pcm, uint32_t generation);

    AudioCodec* codec_ = nullptr;
    TaskHandle_t task_handle_ = nullptr;
    std::mutex init_mutex_;
    std::mutex cache_mutex_;
    std::mutex playback_state_mutex_;
    std::mutex completion_mutex_;
    std::array<CachedCuePcm, kSoundCueCount> cue_cache_ = {};
    std::array<int64_t, kSoundCueCount> last_cue_play_started_us_ = {};
    std::atomic<uint32_t> pending_cue_{static_cast<uint32_t>(SoundCue::kButtonActivate)};
    std::atomic<uint32_t> request_generation_{0};
    uint32_t pending_completion_generation_ = 0;
    std::function<void(SoundCuePlaybackResult)> pending_completion_callback_;
    bool task_started_ = false;
};

}  // namespace system_sound_service_internal

#endif  // SYSTEM_SOUND_SERVICE_INTERNAL_H_
