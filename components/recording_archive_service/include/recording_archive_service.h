#ifndef RECORDING_ARCHIVE_SERVICE_H_
#define RECORDING_ARCHIVE_SERVICE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "esp_err.h"
#include "recording_service.h"

namespace recording_archive_service {

enum class RecordingTag : uint8_t {
    kNote = 0,
    kTask,
    kIdea,
};

// Per-recording metadata persisted as the sidecar JSON next to each clip. This is the
// public, canonical shape; the archive scan/mutation code parses into it directly.
struct RecordingMetadata {
    int version = 1;
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

// A single archived recording, resolved from its sidecar files on the SD card.
// `transcript_text` is loaded eagerly (empty when the recording has no transcript).
struct RecordingEntry {
    std::string recording_id = {};
    std::string recording_path = {};
    std::string transcript_path = {};
    std::string metadata_path = {};
    std::string transcript_text = {};
    int64_t modified_unix_seconds = 0;
    RecordingMetadata metadata = {};
};

struct SaveOptions {
    RecordingTag tag = RecordingTag::kNote;
};

struct SaveResult {
    bool success = false;
    bool clip_saved = false;
    bool metadata_saved = false;
    bool transcript_saved = false;
    std::string recording_id = {};
    std::string status_message = {};
    std::string recording_path = {};
    std::string transcript_path = {};
    std::string metadata_path = {};
    std::string error_code = {};
    std::string error_message = {};
};

// Aggregated counts over the on-SD archive, computed by scanning recording metadata.
// Tag/total counts are real today; completed/follow-up counts are populated by the mutation
// ops below and stay zero until a future page (Todos/Follow-up) flips those flags.
struct Snapshot {
    bool initialized = false;
    bool available = false;
    int recording_count = 0;
    int notes_recording_count = 0;
    int todo_recording_count = 0;
    int follow_up_recording_count = 0;
    int completed_todo_count = 0;
    int incomplete_todo_count = 0;
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

// Seeds the snapshot with an initial archive scan. Safe to call once at startup.
void Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();
// Re-scans the archive and recomputes the snapshot (runs SD I/O on the caller's task; call
// from a non-UI task). Persists the counts to NVS and fires the event handler.
bool Refresh();
// Reset the archive to empty after an SD format (counts -> 0, persisted + notified).
void ResetForFormat();
// Kicks a background scan on a worker task (never blocks the caller). It reconciles the cached
// snapshot against the SD card and only persists + fires an event when the counts actually
// changed. Use this off the boot path (e.g. first home-screen show) to avoid blocking startup.
void RefreshAsync();

// Enumerate every archived recording (Notes + Todos), loading metadata and transcript text.
// Runs SD I/O on the caller's task; call from a non-UI task. Entries are unsorted.
// When the SD read fails, an empty list is returned and *status (if provided) is set to the
// error, so callers can tell a genuinely empty archive apart from a failed read.
std::vector<RecordingEntry> ListRecordings(esp_err_t* status = nullptr);
// Delete a recording and all of its sidecar files (.wav/.json/.txt), then re-aggregate.
bool DeleteRecording(const std::string& recording_id);
// Load an archived recording's WAV back into an in-memory clip (e.g. to re-transcribe it).
// Runs SD I/O on the caller's task; returns nullptr when the clip can't be read.
recording_service::RecordedClipPtr LoadClip(const std::string& recording_id);

// Flip per-recording metadata flags and re-aggregate. Inert until a page invokes them.
bool MarkRecordingCompleted(const std::string& recording_id, bool completed);
bool MarkRecordingFollowUp(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed);
// Change a recording's tag (e.g. turn a Note into a Task) and re-aggregate the archive counts.
bool UpdateRecordingTag(const std::string& recording_id, RecordingTag tag);

SaveResult SaveClip(const recording_service::RecordedClip& clip,
                    const SaveOptions& options = {});
SaveResult SaveTranscript(const std::string& recording_id, const std::string& transcript);

const char* TagName(RecordingTag tag);

}  // namespace recording_archive_service

#endif  // RECORDING_ARCHIVE_SERVICE_H_
