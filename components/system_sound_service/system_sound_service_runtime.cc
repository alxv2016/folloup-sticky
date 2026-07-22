#include "system_sound_service_internal.h"

#include <esp_audio_dec_default.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace system_sound_service_internal {

SystemSoundServiceImpl& SystemSoundServiceImpl::GetInstance() {
    static SystemSoundServiceImpl instance;
    return instance;
}

void SystemSoundServiceImpl::Initialize(AudioCodec* codec) {
    if (codec == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(init_mutex_);
    codec_ = codec;
    if (!task_started_) {
        esp_audio_dec_register_default();
        esp_audio_simple_dec_register_default();
        xTaskCreatePinnedToCore(
            [](void* arg) {
                static_cast<SystemSoundServiceImpl*>(arg)->PlaybackTask();
            },
            "system_sound_service", 8192, this, kPlaybackTaskPriority, &task_handle_,
            kPlaybackTaskCore);
        task_started_ = task_handle_ != nullptr;
    }

    PreloadInteractionCues();
}

void SystemSoundServiceImpl::PlayCue(SoundCue cue) {
    PlayCue(cue, nullptr);
}

void SystemSoundServiceImpl::PlayCue(SoundCue cue,
                               std::function<void(SoundCuePlaybackResult)> on_complete) {
    if (!task_started_ || task_handle_ == nullptr || codec_ == nullptr) {
        DispatchCompletion(std::move(on_complete), SoundCuePlaybackResult::kFailed);
        return;
    }

    const int64_t debounce_window_us = CueDebounceWindowUs(cue);
    if (debounce_window_us > 0) {
        std::lock_guard<std::mutex> lock(playback_state_mutex_);
        const int64_t now_us = esp_timer_get_time();
        const size_t cue_index = CueIndex(cue);
        const int64_t last_play_us = last_cue_play_started_us_[cue_index];
        if (last_play_us > 0 && (now_us - last_play_us) < debounce_window_us) {
            ESP_LOGI(kTag,
                     "Skipping cue '%s' due to debounce window: delta_us=%lld window_us=%lld",
                     CueName(cue), static_cast<long long>(now_us - last_play_us),
                     static_cast<long long>(debounce_window_us));
            DispatchCompletion(std::move(on_complete),
                               SoundCuePlaybackResult::kDebounced);
            return;
        }
        last_cue_play_started_us_[cue_index] = now_us;
    }

    ESP_LOGI(kTag, "Queueing cue '%s'", CueName(cue));

    const uint32_t generation =
        request_generation_.load(std::memory_order_acquire) + 1U;
    ReplacePendingCompletion(generation, std::move(on_complete));
    pending_cue_.store(static_cast<uint32_t>(cue), std::memory_order_release);
    request_generation_.fetch_add(1U, std::memory_order_acq_rel);
    xTaskNotifyGive(task_handle_);
}

void SystemSoundServiceImpl::DispatchCompletion(
    std::function<void(SoundCuePlaybackResult)> callback,
    SoundCuePlaybackResult result) {
    if (!callback) {
        return;
    }
    // folloup-sticky has no shared callback-dispatch task; cue completion callbacks
    // are optional (feedback cues pass none), so invoke directly on the caller.
    callback(result);
}

void SystemSoundServiceImpl::ReplacePendingCompletion(
    uint32_t generation, std::function<void(SoundCuePlaybackResult)> callback) {
    std::function<void(SoundCuePlaybackResult)> superseded_callback;
    {
        std::lock_guard<std::mutex> lock(completion_mutex_);
        if (pending_completion_callback_ &&
            pending_completion_generation_ != generation) {
            superseded_callback = std::move(pending_completion_callback_);
        }
        pending_completion_generation_ = generation;
        pending_completion_callback_ = std::move(callback);
    }
    DispatchCompletion(std::move(superseded_callback),
                       SoundCuePlaybackResult::kSuperseded);
}

void SystemSoundServiceImpl::CompletePendingCompletion(uint32_t generation,
                                                 SoundCuePlaybackResult result) {
    std::function<void(SoundCuePlaybackResult)> callback;
    {
        std::lock_guard<std::mutex> lock(completion_mutex_);
        if (pending_completion_generation_ != generation ||
            !pending_completion_callback_) {
            return;
        }
        callback = std::move(pending_completion_callback_);
        pending_completion_generation_ = 0;
    }
    DispatchCompletion(std::move(callback), result);
}

bool SystemSoundServiceImpl::ShouldInterrupt(uint32_t generation) const {
    return request_generation_.load(std::memory_order_acquire) != generation;
}

void SystemSoundServiceImpl::PreloadInteractionCues() {
    const int64_t start_us = esp_timer_get_time();
    uint32_t warmed = 0;
    for (size_t cue_index = 0; cue_index < kSoundCueCount; ++cue_index) {
        const SoundCue cue = static_cast<SoundCue>(cue_index);
        if (GetOrDecodeCuePcm(cue, request_generation_.load(std::memory_order_acquire)) !=
            nullptr) {
            ++warmed;
        }
    }

    const int64_t elapsed_ms = (esp_timer_get_time() - start_us) / 1000;
    ESP_LOGI(kTag, "Preloaded %u interaction cues in %lld ms",
             static_cast<unsigned>(warmed), static_cast<long long>(elapsed_ms));
}

void SystemSoundServiceImpl::PlaybackTask() {
    uint32_t last_processed_generation = 0;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (codec_ != nullptr) {
            const uint32_t generation = request_generation_.load(std::memory_order_acquire);
            if (generation == last_processed_generation) {
                break;
            }
            const SoundCue cue = static_cast<SoundCue>(
                pending_cue_.load(std::memory_order_acquire));
            PlayCueNow(cue, generation);
            last_processed_generation = generation;
            if (request_generation_.load(std::memory_order_acquire) == generation) {
                break;
            }
        }
    }
}

void SystemSoundServiceImpl::PlayCueNow(SoundCue cue, uint32_t generation) {
    if (codec_ == nullptr) {
        CompletePendingCompletion(generation, SoundCuePlaybackResult::kFailed);
        return;
    }
    if (!codec_->output_enabled()) {
        CompletePendingCompletion(generation, SoundCuePlaybackResult::kFailed);
        return;
    }

    const PsramVector<int16_t>* output_pcm = GetOrDecodeCuePcm(cue, generation);
    if (output_pcm == nullptr || output_pcm->empty()) {
        const SoundCuePlaybackResult result =
            ShouldInterrupt(generation) ? SoundCuePlaybackResult::kInterrupted
                                        : SoundCuePlaybackResult::kFailed;
        CompletePendingCompletion(generation, result);
        return;
    }

    ESP_LOGI(kTag, "Starting cue playback '%s' with %u samples", CueName(cue),
             static_cast<unsigned>(output_pcm->size()));

    bool restore_input = codec_->input_enabled();
    if (restore_input) {
        codec_->EnableInput(false);
    }

    SoundCuePlaybackResult result = SoundCuePlaybackResult::kCompleted;
    for (size_t offset = 0; offset < output_pcm->size() && !ShouldInterrupt(generation);
         offset += kPlaybackChunkSamples) {
        const size_t chunk_size = std::min(output_pcm->size() - offset, kPlaybackChunkSamples);
        if (!codec_->OutputData(output_pcm->data() + offset, chunk_size)) {
            ESP_LOGW(kTag, "Failed writing cue %u at sample offset %u",
                     static_cast<unsigned>(cue), static_cast<unsigned>(offset));
            result = SoundCuePlaybackResult::kFailed;
            break;
        }
    }

    if (result == SoundCuePlaybackResult::kCompleted && ShouldInterrupt(generation)) {
        result = SoundCuePlaybackResult::kInterrupted;
    }

    if (restore_input) {
        codec_->EnableInput(true);
    }

    CompletePendingCompletion(generation, result);
}

}  // namespace system_sound_service_internal
