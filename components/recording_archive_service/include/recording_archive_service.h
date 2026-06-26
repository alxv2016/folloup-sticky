#ifndef RECORDING_ARCHIVE_SERVICE_H_
#define RECORDING_ARCHIVE_SERVICE_H_

#include <cstdint>
#include <string>

#include "recording_service.h"

namespace recording_archive_service {

enum class RecordingTag : uint8_t {
    kNote = 0,
    kTask,
    kIdea,
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
// from a non-UI task). Fires the event handler with the new snapshot.
bool Refresh();

// Flip per-recording metadata flags and re-aggregate. Inert until a page invokes them.
bool MarkRecordingCompleted(const std::string& recording_id, bool completed);
bool MarkRecordingFollowUp(const std::string& recording_id, bool follow_up,
                           bool follow_up_completed);

SaveResult SaveClip(const recording_service::RecordedClip& clip,
                    const SaveOptions& options = {});
SaveResult SaveTranscript(const std::string& recording_id, const std::string& transcript);

const char* TagName(RecordingTag tag);

}  // namespace recording_archive_service

#endif  // RECORDING_ARCHIVE_SERVICE_H_
