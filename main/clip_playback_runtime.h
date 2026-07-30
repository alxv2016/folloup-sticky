#ifndef CLIP_PLAYBACK_RUNTIME_H_
#define CLIP_PLAYBACK_RUNTIME_H_

#include <string>

namespace clip_playback_runtime {

// Streams a recorded WAV to the speaker without blocking the caller.
//
// playback_service::PlayFile blocks for the length of the clip, and every caller here
// reaches it from the input-dispatch path (a select-modal submit), which must not stall.
// This runs it on a short-lived worker and serializes to one clip at a time, so a second
// request while one is already playing is ignored rather than queued.
//
// Returns false if the path is empty, a clip is already playing, or the worker could not
// be started.
bool PlayFileAsync(const std::string& path);

}  // namespace clip_playback_runtime

#endif  // CLIP_PLAYBACK_RUNTIME_H_
