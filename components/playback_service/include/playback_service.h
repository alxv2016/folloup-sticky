#ifndef PLAYBACK_SERVICE_H_
#define PLAYBACK_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

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

}  // namespace playback_service

#endif  // PLAYBACK_SERVICE_H_
