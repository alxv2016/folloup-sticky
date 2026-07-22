#include "system_sound_service_internal.h"

#include <esp_audio_dec_default.h>
#include <esp_audio_simple_dec.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_log.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace system_sound_service_internal {

namespace {

int16_t ClampI32ToI16(int32_t value) {
    return static_cast<int16_t>(std::clamp(value, static_cast<int32_t>(INT16_MIN),
                                           static_cast<int32_t>(INT16_MAX)));
}

int16_t InterpolateSample(int16_t left, int16_t right, double fraction) {
    double blended = static_cast<double>(left) +
                     (static_cast<double>(right) - static_cast<double>(left)) * fraction;
    blended = std::clamp(blended, static_cast<double>(INT16_MIN), static_cast<double>(INT16_MAX));
    return static_cast<int16_t>(std::lround(blended));
}

template <typename SampleContainer>
int16_t DownmixFrameToMono(const SampleContainer& interleaved, uint8_t channels,
                           size_t frame_index) {
    if (channels == 0) {
        return 0;
    }

    const size_t base = frame_index * channels;
    int32_t total = 0;
    for (uint8_t channel = 0; channel < channels; ++channel) {
        total += interleaved[base + channel];
    }
    return ClampI32ToI16(total / channels);
}

template <typename SampleContainer>
PsramVector<int16_t> ConvertToOutputPcm(const SampleContainer& interleaved,
                                        uint32_t sample_rate_hz, uint8_t channels) {
    PsramVector<int16_t> mono_output;
    if (interleaved.empty() || sample_rate_hz == 0 || channels == 0) {
        return mono_output;
    }

    const size_t frame_count = interleaved.size() / channels;
    if (frame_count == 0) {
        return mono_output;
    }

    if (sample_rate_hz == kOutputSampleRateHz && channels == 1) {
        mono_output.assign(interleaved.begin(), interleaved.end());
        return mono_output;
    }

    if (sample_rate_hz == kOutputSampleRateHz) {
        mono_output.reserve(frame_count);
        for (size_t frame_index = 0; frame_index < frame_count; ++frame_index) {
            mono_output.push_back(DownmixFrameToMono(interleaved, channels, frame_index));
        }
        return mono_output;
    }

    const double source_step =
        static_cast<double>(sample_rate_hz) / static_cast<double>(kOutputSampleRateHz);
    mono_output.reserve(
        static_cast<size_t>((static_cast<uint64_t>(frame_count) * kOutputSampleRateHz) /
                            sample_rate_hz) +
        2U);

    double source_index = 0.0;
    while (source_index + 1.0 < static_cast<double>(frame_count)) {
        const size_t left_index = static_cast<size_t>(source_index);
        const double fraction = source_index - static_cast<double>(left_index);
        mono_output.push_back(InterpolateSample(
            DownmixFrameToMono(interleaved, channels, left_index),
            DownmixFrameToMono(interleaved, channels, left_index + 1U), fraction));
        source_index += source_step;
    }

    if (mono_output.empty()) {
        mono_output.push_back(DownmixFrameToMono(interleaved, channels, 0));
    }

    return mono_output;
}

}  // namespace

const PsramVector<int16_t>* SystemSoundServiceImpl::GetOrDecodeCuePcm(SoundCue cue,
                                                                uint32_t generation) {
    const size_t cue_index = CueIndex(cue);
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        if (cue_cache_[cue_index].ready) {
            return &cue_cache_[cue_index].samples;
        }
    }

    const EmbeddedMp3 mp3 = CueMp3(cue);
    if (mp3.data == nullptr || mp3.size == 0) {
        ESP_LOGW(kTag, "No sound data found for cue %u", static_cast<unsigned>(cue));
        return nullptr;
    }
    std::vector<uint8_t> encoded(mp3.data, mp3.data + mp3.size);

    PsramVector<int16_t> decoded_pcm;
    if (!DecodeMp3ToOutputPcm(encoded, &decoded_pcm, generation) || decoded_pcm.empty()) {
        return nullptr;
    }
    if (ShouldInterrupt(generation)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(cache_mutex_);
    CachedCuePcm& cache_entry = cue_cache_[cue_index];
    if (!cache_entry.ready) {
        cache_entry.samples = std::move(decoded_pcm);
        cache_entry.ready = true;
    }
    return &cache_entry.samples;
}

bool SystemSoundServiceImpl::DecodeMp3ToOutputPcm(const std::vector<uint8_t>& encoded,
                                            PsramVector<int16_t>* output_pcm,
                                            uint32_t generation) {
    if (output_pcm == nullptr) {
        return false;
    }

    esp_audio_simple_dec_cfg_t cfg = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = nullptr,
        .cfg_size = 0,
        .use_frame_dec = false,
    };

    esp_audio_simple_dec_handle_t decoder = nullptr;
    esp_audio_err_t ret = esp_audio_simple_dec_open(&cfg, &decoder);
    if (ret != ESP_AUDIO_ERR_OK || decoder == nullptr) {
        ESP_LOGW(kTag, "Failed to open MP3 decoder: %d", ret);
        return false;
    }

    bool success = false;
    bool info_ready = false;
    esp_audio_simple_dec_info_t info = {};
    std::vector<uint8_t> decode_bytes(kInitialDecoderOutputBytes);
    PsramVector<int16_t> decoded_interleaved;

    for (size_t offset = 0; offset < encoded.size() && !ShouldInterrupt(generation);
         offset += kDecoderInputChunkBytes) {
        const size_t feed_size = std::min(encoded.size() - offset, kDecoderInputChunkBytes);
        esp_audio_simple_dec_raw_t raw = {
            .buffer = const_cast<uint8_t*>(encoded.data() + offset),
            .len = static_cast<uint32_t>(feed_size),
            .eos = (offset + feed_size) >= encoded.size(),
            .consumed = 0,
            .frame_recover = ESP_AUDIO_SIMPLE_DEC_RECOVERY_NONE,
        };

        while (raw.len > 0 && !ShouldInterrupt(generation)) {
            esp_audio_simple_dec_out_t out = {
                .buffer = decode_bytes.data(),
                .len = static_cast<uint32_t>(decode_bytes.size()),
                .needed_size = 0,
                .decoded_size = 0,
            };
            ret = esp_audio_simple_dec_process(decoder, &raw, &out);
            if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                decode_bytes.resize(out.needed_size);
                continue;
            }
            if (ret != ESP_AUDIO_ERR_OK) {
                ESP_LOGW(kTag, "MP3 decode failed: %d", ret);
                goto cleanup;
            }

            if (out.decoded_size > 0) {
                if (!info_ready) {
                    ret = esp_audio_simple_dec_get_info(decoder, &info);
                    if (ret != ESP_AUDIO_ERR_OK) {
                        ESP_LOGW(kTag, "Failed to read MP3 stream info: %d", ret);
                        goto cleanup;
                    }
                    info_ready = true;
                    if (info.bits_per_sample != 16 || info.channel == 0) {
                        ESP_LOGW(kTag,
                                 "Unsupported MP3 output format: %u bits, %u channels",
                                 static_cast<unsigned>(info.bits_per_sample),
                                 static_cast<unsigned>(info.channel));
                        goto cleanup;
                    }
                }

                const size_t sample_count = out.decoded_size / sizeof(int16_t);
                const size_t previous_size = decoded_interleaved.size();
                decoded_interleaved.resize(previous_size + sample_count);
                memcpy(decoded_interleaved.data() + previous_size, out.buffer,
                       out.decoded_size);
            }

            if (raw.consumed == 0 && out.decoded_size == 0) {
                ESP_LOGW(kTag, "Decoder stalled while processing MP3 stream");
                goto cleanup;
            }

            raw.len -= raw.consumed;
            raw.buffer += raw.consumed;
            raw.consumed = 0;
        }
    }

    if (ShouldInterrupt(generation) || !info_ready || decoded_interleaved.empty()) {
        goto cleanup;
    }

    *output_pcm =
        ConvertToOutputPcm(decoded_interleaved, info.sample_rate, info.channel);
    success = !output_pcm->empty();

cleanup:
    esp_audio_simple_dec_close(decoder);
    return success;
}

}  // namespace system_sound_service_internal
