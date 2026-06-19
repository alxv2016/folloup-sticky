#ifndef RECORDING_SERVICE_H_
#define RECORDING_SERVICE_H_

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <vector>

#include "esp_err.h"
#include "esp_heap_caps.h"

namespace recording_service {

enum class State : uint8_t {
    kIdle = 0,
    kArmed,
    kRecording,
    kClipReady,
};

enum class StopReason : uint8_t {
    kNone = 0,
    kFinished,
    kCanceled,
    kMaxDuration,
    kError,
};

enum class StartMode : uint8_t {
    kIncludePreroll = 0,
    kFresh,
};

struct UiState {
    bool initialized = false;
    bool armed = false;
    bool recording = false;
    bool has_clip = false;
    size_t recorded_samples = 0;
    uint32_t duration_ms = 0;
    uint32_t max_recording_ms = 0;
    uint8_t input_level_percent = 0;
    StopReason stop_reason = StopReason::kNone;
};

struct Event {
    State state = State::kIdle;
    UiState ui_state = {};
};

template <typename T>
class PsramAllocator {
public:
    using value_type = T;

    PsramAllocator() noexcept = default;

    template <typename U>
    PsramAllocator(const PsramAllocator<U>&) noexcept
    {
    }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            std::abort();
        }

        void* memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) {
            memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_8BIT);
        }
        if (memory == nullptr) {
            std::abort();
        }
        return static_cast<T*>(memory);
    }

    void deallocate(T* pointer, std::size_t) noexcept
    {
        heap_caps_free(pointer);
    }

    template <typename U>
    bool operator==(const PsramAllocator<U>&) const noexcept
    {
        return true;
    }

    template <typename U>
    bool operator!=(const PsramAllocator<U>&) const noexcept
    {
        return false;
    }
};

template <typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;

class RecordedClip {
public:
    using Chunk = PsramVector<int16_t>;
    using ChunkList = std::vector<Chunk>;

    RecordedClip(uint32_t sample_rate_hz, size_t sample_count, ChunkList chunks);

    uint32_t sample_rate_hz() const { return sample_rate_hz_; }
    size_t sample_count() const { return sample_count_; }
    size_t pcm16_byte_count() const { return sample_count_ * sizeof(int16_t); }
    size_t wav_byte_count() const { return pcm16_byte_count() + 44; }
    bool empty() const { return sample_count_ == 0; }
    uint32_t duration_ms() const;

    void ForEachChunk(const std::function<void(const int16_t*, size_t)>& visitor) const;

private:
    uint32_t sample_rate_hz_ = 0;
    size_t sample_count_ = 0;
    ChunkList chunks_;
};

using RecordedClipPtr = std::shared_ptr<RecordedClip>;
using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
bool IsInitialized();
void SetEventHandler(EventHandler handler, void* context);
UiState GetUiState();

esp_err_t Arm();
esp_err_t Start(StartMode mode = StartMode::kIncludePreroll);
esp_err_t Finish();
esp_err_t Cancel();
void DiscardClip();
RecordedClipPtr GetRecordedClip();

esp_err_t SaveLastClipWav(const char* path);
esp_err_t RecordDebugClipToWav(const char* path, uint32_t lead_in_ms = 1000,
                               uint32_t record_ms = 2500);
void LogDebugStatus();

}  // namespace recording_service

#endif  // RECORDING_SERVICE_H_
