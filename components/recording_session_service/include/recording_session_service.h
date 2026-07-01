#ifndef RECORDING_SESSION_SERVICE_H_
#define RECORDING_SESSION_SERVICE_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "esp_err.h"
#include "recording_archive_service.h"
#include "recording_service.h"
#include "transcription_service.h"

namespace recording_session_service {

enum class Phase : uint8_t {
    kIdle = 0,
    kArmed,
    kRecording,
    kAwaitingTagSelection,
    kSaving,
    kTranscribing,
    kComplete,
    kFailed,
};

enum class BlockedReason : uint8_t {
    kNone = 0,
    kLockScreenActive,
    kOverlayVisible,
    kStorageBusy,
    kRecorderUnavailable,
    kTranscriptionInFlight,
};

struct Context {
    bool lock_screen_active = false;
    bool overlay_visible = false;
};

struct Snapshot {
    bool initialized = false;
    bool allowed = false;
    BlockedReason blocked_reason = BlockedReason::kNone;
    Phase phase = Phase::kIdle;
    bool has_clip = false;
    bool clip_saved = false;
    bool transcript_saved = false;
    bool request_in_flight = false;
    size_t recorded_samples = 0;
    uint32_t duration_ms = 0;
    uint32_t max_recording_ms = 0;
    uint8_t input_level_percent = 0;
    recording_service::StopReason stop_reason = recording_service::StopReason::kNone;
    std::string last_saved_recording_id = {};
    std::string last_saved_recording_path = {};
    std::string last_saved_transcript_path = {};
    std::string last_transcript = {};
    std::string last_status_message = {};
    std::string last_error_code = {};
    std::string last_error_message = {};
};

struct Event {
    Snapshot snapshot = {};
};

struct TagOption {
    recording_archive_service::RecordingTag tag = recording_archive_service::RecordingTag::kNote;
    std::string_view label_text = {};
    // When true, selecting this option throws the recording away instead of
    // archiving it; the `tag` field is unused. Marks intent explicitly so the
    // discard path can't break if the option order changes.
    bool is_discard = false;
};

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();
const std::array<TagOption, 4>& TagOptions();

bool HandlePowerPressDown(const Context& context);
bool HandlePowerLongPressStart(const Context& context);
bool HandlePowerPressUp(const Context& context);
bool SubmitTagSelection(int selected_index);

void HandleRecordingEvent(const recording_service::Event& event);
void HandleTranscriptionEvent(const transcription_service::Event& event);

const char* PhaseName(Phase phase);

}  // namespace recording_session_service

#endif  // RECORDING_SESSION_SERVICE_H_
