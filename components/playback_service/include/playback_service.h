#ifndef PLAYBACK_SERVICE_H_
#define PLAYBACK_SERVICE_H_

#include <cstdint>

#include "esp_err.h"
#include "recording_service.h"

// Streams a recorded PCM WAV clip out through the ES8311 codec (the board-owned
// AudioCodec). Playback is a discrete, user-initiated action, so this is a simple
// blocking call intended to run on a worker task; the codec output/PA are enabled
// for the duration of the clip and disabled when it finishes.
namespace playback_service {

// True while a clip is actively being streamed to the codec.
bool IsPlaying();

// Request the current playback (if any) to stop early; PlayFile returns shortly
// after.
void Stop();

// Plays the 16 kHz mono/16-bit PCM WAV at `path` (e.g. "/sdcard/...") to the
// speaker. Blocks until the clip finishes, an error occurs, or Stop() is called.
// Returns ESP_ERR_INVALID_STATE if the codec is unavailable or already playing.
esp_err_t PlayFile(const char* path);

// Plays a clip straight from the PSRAM chunks recording_service already holds, with no
// SD round-trip. This is what the review-after-recording step uses: the clip has not been
// saved at that point, precisely so a bad take can be discarded without ever hitting the
// card. Blocks like PlayFile, and returns ESP_ERR_INVALID_STATE if the codec is
// unavailable or playback is already running.
esp_err_t PlayClip(const recording_service::RecordedClipPtr& clip);

}  // namespace playback_service

#endif  // PLAYBACK_SERVICE_H_
