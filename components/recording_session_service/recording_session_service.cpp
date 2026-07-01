#include "recording_session_service.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <string>

#include "esp_log.h"
#include "gemini_service.h"
#include "storage_service.h"

namespace recording_session_service {
namespace {

constexpr const char* kTag = "RecordingSession";
constexpr const char* kIdleStatus = "Hold POWER to record";
constexpr const char* kArmedStatus = "Keep holding to record";
constexpr const char* kRecordingStatus = "Recording";
constexpr const char* kChooseTagStatus = "Choose recording type";
constexpr const char* kSavingStatus = "Saving recording";
constexpr const char* kTranscribingStatus = "Transcribing recording";
constexpr const char* kSavedWithoutTranscriptStatus =
    "Recording saved without transcription";
constexpr const char* kDiscardedStatus = "Recording discarded";
constexpr uint32_t kMinTranscriptionDurationMs = 500;
constexpr uint32_t kFallbackAudioSampleRateHz = 24000;
constexpr size_t kSignalWindowSamples = 240;
constexpr int32_t kSpeechPeakThreshold = 700;
constexpr size_t kMinSpeechWindows = 3;

constexpr std::array<TagOption, 4> kTagOptions = {{
    {.tag = recording_archive_service::RecordingTag::kNote, .label_text = "Note"},
    {.tag = recording_archive_service::RecordingTag::kTask, .label_text = "Task"},
    {.tag = recording_archive_service::RecordingTag::kIdea, .label_text = "Idea"},
    {.label_text = "Discard", .is_discard = true},
}};

struct GuardrailResult {
    bool accepted = false;
    const char* error_code = "";
    const char* status_message = "";
};

std::mutex s_mutex;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
bool s_initialized = false;
Snapshot s_snapshot = {};
std::string s_pending_recording_id = {};

uint32_t ResolveClipDurationMs(const recording_service::RecordedClip& clip, uint32_t duration_ms)
{
    if (duration_ms > 0) {
        return duration_ms;
    }
    return clip.sample_rate_hz() > 0
               ? clip.duration_ms()
               : static_cast<uint32_t>((clip.sample_count() * 1000ULL) /
                                       kFallbackAudioSampleRateHz);
}

GuardrailResult ValidateClip(const recording_service::RecordedClip& clip, uint32_t duration_ms)
{
    if (clip.empty()) {
        return {
            .accepted = false,
            .error_code = "empty_audio",
            .status_message = "Recording too short",
        };
    }

    const uint32_t resolved_duration_ms = ResolveClipDurationMs(clip, duration_ms);
    if (resolved_duration_ms < kMinTranscriptionDurationMs) {
        return {
            .accepted = false,
            .error_code = "recording_too_short",
            .status_message = "Recording too short",
        };
    }

    size_t speech_windows = 0;
    size_t window_fill = 0;
    int32_t window_peak = 0;
    auto finish_window = [&]() -> bool {
        if (window_fill == 0) {
            return false;
        }
        if (window_peak >= kSpeechPeakThreshold) {
            ++speech_windows;
            if (speech_windows >= kMinSpeechWindows) {
                return true;
            }
        }
        window_fill = 0;
        window_peak = 0;
        return false;
    };

    bool accepted = false;
    clip.ForEachChunk([&](const int16_t* chunk_data, size_t chunk_size) {
        if (accepted || chunk_data == nullptr) {
            return;
        }
        for (size_t index = 0; index < chunk_size; ++index) {
            int32_t amplitude = chunk_data[index];
            if (amplitude < 0) {
                amplitude = -amplitude;
            }
            window_peak = std::max(window_peak, amplitude);
            ++window_fill;
            if (window_fill >= kSignalWindowSamples && finish_window()) {
                accepted = true;
                break;
            }
        }
    });
    if (!accepted && finish_window()) {
        accepted = true;
    }

    if (accepted) {
        return {
            .accepted = true,
            .error_code = "",
            .status_message = "",
        };
    }

    return {
        .accepted = false,
        .error_code = "recording_too_quiet",
        .status_message = "No speech detected",
    };
}

BlockedReason EvaluateBlockedReason(const Context& context)
{
    if (context.lock_screen_active) {
        return BlockedReason::kLockScreenActive;
    }
    if (context.overlay_visible) {
        return BlockedReason::kOverlayVisible;
    }
    if (storage_service::IsWriteBusy()) {
        return BlockedReason::kStorageBusy;
    }
    if (!recording_service::IsInitialized()) {
        return BlockedReason::kRecorderUnavailable;
    }
    if (transcription_service::GetSnapshot().request_in_flight) {
        return BlockedReason::kTranscriptionInFlight;
    }
    return BlockedReason::kNone;
}

const char* BlockedReasonStatusMessage(BlockedReason reason)
{
    switch (reason) {
        case BlockedReason::kLockScreenActive:
            return "Unlock to record";
        case BlockedReason::kOverlayVisible:
            return "Close the current dialog";
        case BlockedReason::kStorageBusy:
            return "Wait for SD activity to finish";
        case BlockedReason::kRecorderUnavailable:
            return "Recorder unavailable";
        case BlockedReason::kTranscriptionInFlight:
            return "Wait for transcription to finish";
        case BlockedReason::kNone:
        default:
            return "";
    }
}

void ResetToIdleLocked()
{
    s_snapshot.allowed = true;
    s_snapshot.blocked_reason = BlockedReason::kNone;
    s_snapshot.phase = Phase::kIdle;
    s_snapshot.has_clip = false;
    s_snapshot.clip_saved = false;
    s_snapshot.transcript_saved = false;
    s_snapshot.request_in_flight = false;
    s_snapshot.recorded_samples = 0;
    s_snapshot.duration_ms = 0;
    s_snapshot.input_level_percent = 0;
    s_snapshot.stop_reason = recording_service::StopReason::kNone;
    s_snapshot.last_saved_recording_id.clear();
    s_snapshot.last_saved_recording_path.clear();
    s_snapshot.last_saved_transcript_path.clear();
    s_snapshot.last_transcript.clear();
    s_snapshot.last_error_code.clear();
    s_snapshot.last_error_message.clear();
    s_snapshot.last_status_message = kIdleStatus;
    s_pending_recording_id.clear();
}

void SyncRecordingStateLocked(const recording_service::UiState& state)
{
    s_snapshot.initialized = state.initialized;
    s_snapshot.recorded_samples = state.recorded_samples;
    s_snapshot.duration_ms = state.duration_ms;
    s_snapshot.max_recording_ms = state.max_recording_ms;
    s_snapshot.input_level_percent = state.input_level_percent;
    s_snapshot.stop_reason = state.stop_reason;
    s_snapshot.has_clip = state.has_clip;
}

void NotifyLocked()
{
    EventHandler handler = s_event_handler;
    void* context = s_event_context;
    if (handler == nullptr) {
        return;
    }

    const Event event = {
        .snapshot = s_snapshot,
    };
    handler(event, context);
}

void MarkBlockedLocked(BlockedReason reason)
{
    s_snapshot.allowed = false;
    s_snapshot.blocked_reason = reason;
    s_snapshot.last_status_message = BlockedReasonStatusMessage(reason);
    s_snapshot.last_error_code.clear();
    s_snapshot.last_error_message.clear();
}

}  // namespace

esp_err_t Init()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_initialized) {
        return ESP_OK;
    }

    s_initialized = true;
    s_snapshot.initialized = recording_service::IsInitialized();
    s_snapshot.max_recording_ms = recording_service::GetUiState().max_recording_ms;
    ResetToIdleLocked();
    return ESP_OK;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

Snapshot GetSnapshot()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_snapshot;
}

const std::array<TagOption, 4>& TagOptions()
{
    return kTagOptions;
}

bool BeginArchivedTranscription(const std::string& recording_id)
{
    if (Init() != ESP_OK || recording_id.empty()) {
        return false;
    }

    // Reuse the recording->transcription pipeline for an already-archived clip: load its WAV
    // back into memory, hand it to the same transcription service, and let the existing
    // HandleTranscriptionEvent path save the transcript and drive the toasts.
    if (transcription_service::GetSnapshot().request_in_flight) {
        return false;  // a recording or another re-transcribe is already running
    }

    recording_service::RecordedClipPtr clip = recording_archive_service::LoadClip(recording_id);
    if (!clip || clip->empty()) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kFailed;
        s_snapshot.request_in_flight = false;
        s_snapshot.last_status_message = "Couldn't load recording audio";
        s_snapshot.last_error_code = "clip_load_failed";
        s_snapshot.last_error_message = "Failed to read WAV from SD";
        NotifyLocked();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.clip_saved = true;  // the recording already lives on SD
        s_snapshot.transcript_saved = false;
        s_pending_recording_id = recording_id;
    }

    if (transcription_service::BeginTranscription(clip)) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kTranscribing;
        s_snapshot.request_in_flight = true;
        s_snapshot.last_status_message = kTranscribingStatus;
        s_snapshot.last_error_code.clear();
        s_snapshot.last_error_message.clear();
        NotifyLocked();
        return true;
    }

    // Couldn't start (e.g. Gemini not ready): surface the transcription error as a failure.
    const transcription_service::Snapshot ts = transcription_service::GetSnapshot();
    std::lock_guard<std::mutex> lock(s_mutex);
    s_snapshot.phase = Phase::kFailed;
    s_snapshot.request_in_flight = false;
    s_snapshot.last_status_message =
        ts.last_status_message.empty() ? "Transcription unavailable" : ts.last_status_message;
    s_snapshot.last_error_code = ts.last_error_code;
    s_snapshot.last_error_message = ts.last_error_message;
    s_pending_recording_id.clear();
    NotifyLocked();
    return false;
}

bool HandlePowerPressDown(const Context& context)
{
    if (Init() != ESP_OK) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_snapshot.phase == Phase::kAwaitingTagSelection ||
            s_snapshot.phase == Phase::kTranscribing) {
            return false;
        }

        const BlockedReason blocked_reason = EvaluateBlockedReason(context);
        if (blocked_reason != BlockedReason::kNone) {
            MarkBlockedLocked(blocked_reason);
            NotifyLocked();
            return false;
        }
    }

    const esp_err_t err = recording_service::Arm();
    std::lock_guard<std::mutex> lock(s_mutex);
    if (err != ESP_OK) {
        s_snapshot.phase = Phase::kFailed;
        s_snapshot.allowed = false;
        s_snapshot.blocked_reason = BlockedReason::kRecorderUnavailable;
        s_snapshot.last_status_message = "Recorder unavailable";
        s_snapshot.last_error_code = "record_arm_failed";
        s_snapshot.last_error_message = esp_err_to_name(err);
        NotifyLocked();
        return false;
    }

    ResetToIdleLocked();
    s_snapshot.phase = Phase::kArmed;
    s_snapshot.allowed = true;
    s_snapshot.last_status_message = kArmedStatus;
    NotifyLocked();
    return true;
}

bool HandlePowerLongPressStart(const Context& context)
{
    if (Init() != ESP_OK) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const BlockedReason blocked_reason = EvaluateBlockedReason(context);
        if (blocked_reason != BlockedReason::kNone) {
            MarkBlockedLocked(blocked_reason);
            NotifyLocked();
            return false;
        }
        if (s_snapshot.phase != Phase::kArmed) {
            return false;
        }
    }

    const esp_err_t err = recording_service::Start(recording_service::StartMode::kFresh);
    std::lock_guard<std::mutex> lock(s_mutex);
    if (err != ESP_OK) {
        s_snapshot.phase = Phase::kFailed;
        s_snapshot.last_status_message = "Recording failed to start";
        s_snapshot.last_error_code = "record_start_failed";
        s_snapshot.last_error_message = esp_err_to_name(err);
        NotifyLocked();
        return false;
    }

    s_snapshot.phase = Phase::kRecording;
    s_snapshot.last_status_message = kRecordingStatus;
    s_snapshot.last_error_code.clear();
    s_snapshot.last_error_message.clear();
    NotifyLocked();
    return true;
}

bool HandlePowerPressUp(const Context&)
{
    if (Init() != ESP_OK) {
        return false;
    }

    Phase phase = Phase::kIdle;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        phase = s_snapshot.phase;
    }

    if (phase == Phase::kArmed) {
        (void)recording_service::Cancel();
        std::lock_guard<std::mutex> lock(s_mutex);
        ResetToIdleLocked();
        NotifyLocked();
        return true;
    }

    if (phase != Phase::kRecording) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kSaving;
        s_snapshot.last_status_message = "Preparing recording";
        NotifyLocked();
    }
    (void)recording_service::Finish();
    return true;
}

bool SubmitTagSelection(int selected_index)
{
    if (Init() != ESP_OK) {
        return false;
    }

    if (selected_index < 0 || selected_index >= static_cast<int>(kTagOptions.size())) {
        return false;
    }

    ESP_LOGI(kTag,
             "Recording tag selection submitted: index=%d label=%.*s",
             selected_index,
             static_cast<int>(kTagOptions[static_cast<size_t>(selected_index)].label_text.size()),
             kTagOptions[static_cast<size_t>(selected_index)].label_text.data());

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_snapshot.phase != Phase::kAwaitingTagSelection) {
            return false;
        }
    }

    if (kTagOptions[static_cast<size_t>(selected_index)].is_discard) {
        recording_service::DiscardClip();
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kComplete;
        s_snapshot.has_clip = false;
        s_snapshot.clip_saved = false;
        s_snapshot.transcript_saved = false;
        s_snapshot.last_status_message = kDiscardedStatus;
        s_snapshot.last_error_code.clear();
        s_snapshot.last_error_message.clear();
        NotifyLocked();
        return true;
    }

    recording_service::RecordedClipPtr clip = recording_service::GetRecordedClip();
    if (!clip || clip->empty()) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kFailed;
        s_snapshot.last_status_message = "Save failed";
        s_snapshot.last_error_code = "recording_missing";
        s_snapshot.last_error_message = "Recording clip was not available";
        NotifyLocked();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kSaving;
        s_snapshot.last_status_message = kSavingStatus;
        s_snapshot.last_error_code.clear();
        s_snapshot.last_error_message.clear();
        NotifyLocked();
    }

    recording_archive_service::SaveOptions options = {};
    options.tag = kTagOptions[static_cast<size_t>(selected_index)].tag;
    ESP_LOGI(kTag,
             "Starting archive save: tag=%s samples=%u duration_ms=%lu",
             recording_archive_service::TagName(options.tag),
             static_cast<unsigned>(clip->sample_count()),
             static_cast<unsigned long>(clip->duration_ms()));
    const recording_archive_service::SaveResult save_result =
        recording_archive_service::SaveClip(*clip, options);
    ESP_LOGI(kTag,
             "Archive save result: success=%d clip_saved=%d metadata_saved=%d id=%s wav=%s metadata=%s error=%s",
             save_result.success ? 1 : 0,
             save_result.clip_saved ? 1 : 0,
             save_result.metadata_saved ? 1 : 0,
             save_result.recording_id.empty() ? "<none>" : save_result.recording_id.c_str(),
             save_result.recording_path.empty() ? "<none>" : save_result.recording_path.c_str(),
             save_result.metadata_path.empty() ? "<none>" : save_result.metadata_path.c_str(),
             save_result.error_code.empty() ? "<none>" : save_result.error_code.c_str());

    const bool should_transcribe = save_result.clip_saved && gemini_service::GetSnapshot().runtime.ready;
    ESP_LOGI(kTag,
             "Transcription decision: clip_saved=%d gemini_ready=%d should_transcribe=%d",
             save_result.clip_saved ? 1 : 0,
             gemini_service::GetSnapshot().runtime.ready ? 1 : 0,
             should_transcribe ? 1 : 0);

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.clip_saved = save_result.clip_saved;
        s_snapshot.transcript_saved = false;
        s_snapshot.last_saved_recording_id = save_result.recording_id;
        s_snapshot.last_saved_recording_path = save_result.recording_path;
        s_snapshot.last_saved_transcript_path = save_result.transcript_path;
        s_pending_recording_id = save_result.recording_id;
    }

    if (should_transcribe && transcription_service::BeginTranscription(clip)) {
        recording_service::DiscardClip();
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.phase = Phase::kTranscribing;
        s_snapshot.request_in_flight = true;
        s_snapshot.last_status_message = kTranscribingStatus;
        s_snapshot.last_error_code.clear();
        s_snapshot.last_error_message.clear();
        NotifyLocked();
        return true;
    }

    if (should_transcribe) {
        ESP_LOGW(kTag,
                 "Transcription did not start after save: id=%s",
                 save_result.recording_id.empty() ? "<none>" : save_result.recording_id.c_str());
    }

    recording_service::DiscardClip();
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.request_in_flight = false;
        s_snapshot.phase = save_result.clip_saved ? Phase::kComplete : Phase::kFailed;
        s_snapshot.last_status_message = save_result.clip_saved
                                             ? kSavedWithoutTranscriptStatus
                                             : (save_result.status_message.empty()
                                                    ? "Save failed"
                                                    : save_result.status_message);
        s_snapshot.last_error_code = save_result.error_code;
        s_snapshot.last_error_message = save_result.error_message;
        if (save_result.clip_saved && !save_result.error_code.empty()) {
            // Keep the saved result but surface the archive warning.
            s_snapshot.last_error_message = save_result.error_message;
        } else if (save_result.clip_saved) {
            s_snapshot.last_error_code.clear();
            s_snapshot.last_error_message.clear();
        }
        NotifyLocked();
    }
    return save_result.clip_saved;
}

void HandleRecordingEvent(const recording_service::Event& event)
{
    if (Init() != ESP_OK) {
        return;
    }

    recording_service::RecordedClipPtr clip = nullptr;
    bool should_show_tag_selection = false;
    GuardrailResult guardrail = {};
    bool discard_invalid_clip = false;

    if (event.state == recording_service::State::kClipReady && event.ui_state.has_clip) {
        clip = recording_service::GetRecordedClip();
        if (clip) {
            guardrail = ValidateClip(*clip, event.ui_state.duration_ms);
            should_show_tag_selection = guardrail.accepted;
        }
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        SyncRecordingStateLocked(event.ui_state);
        s_snapshot.request_in_flight = transcription_service::GetSnapshot().request_in_flight;

        if (event.state == recording_service::State::kArmed && s_snapshot.phase == Phase::kIdle) {
            s_snapshot.phase = Phase::kArmed;
            s_snapshot.last_status_message = kArmedStatus;
        } else if (event.state == recording_service::State::kRecording) {
            s_snapshot.phase = Phase::kRecording;
            s_snapshot.last_status_message = kRecordingStatus;
        } else if (event.state == recording_service::State::kClipReady) {
            if (should_show_tag_selection) {
                s_snapshot.phase = Phase::kAwaitingTagSelection;
                s_snapshot.last_status_message = kChooseTagStatus;
                s_snapshot.last_error_code.clear();
                s_snapshot.last_error_message.clear();
            } else {
                discard_invalid_clip = true;
                s_snapshot.phase = Phase::kFailed;
                s_snapshot.has_clip = false;
                s_snapshot.last_status_message = guardrail.status_message;
                s_snapshot.last_error_code = guardrail.error_code;
                s_snapshot.last_error_message = guardrail.status_message;
            }
        }

        NotifyLocked();
    }
    if (discard_invalid_clip) {
        recording_service::DiscardClip();
    }
}

void HandleTranscriptionEvent(const transcription_service::Event& event)
{
    if (Init() != ESP_OK) {
        return;
    }

    bool should_attach_transcript = false;
    std::string pending_recording_id;
    std::string transcript_text;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.request_in_flight = event.snapshot.request_in_flight;
        if (s_snapshot.phase == Phase::kTranscribing && !event.snapshot.request_in_flight &&
            !event.snapshot.last_transcript.empty() && !s_pending_recording_id.empty()) {
            should_attach_transcript = true;
            pending_recording_id = s_pending_recording_id;
            transcript_text = event.snapshot.last_transcript;
        }
    }

    if (should_attach_transcript) {
        ESP_LOGI(kTag,
                 "Attaching transcript to saved recording: id=%s chars=%u",
                 pending_recording_id.c_str(),
                 static_cast<unsigned>(transcript_text.size()));
        const recording_archive_service::SaveResult save_result =
            recording_archive_service::SaveTranscript(pending_recording_id, transcript_text);
        ESP_LOGI(kTag,
                 "Transcript save result: success=%d transcript_saved=%d metadata_saved=%d transcript=%s metadata=%s error=%s",
                 save_result.success ? 1 : 0,
                 save_result.transcript_saved ? 1 : 0,
                 save_result.metadata_saved ? 1 : 0,
                 save_result.transcript_path.empty() ? "<none>" : save_result.transcript_path.c_str(),
                 save_result.metadata_path.empty() ? "<none>" : save_result.metadata_path.c_str(),
                 save_result.error_code.empty() ? "<none>" : save_result.error_code.c_str());
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.transcript_saved = save_result.transcript_saved;
        s_snapshot.last_saved_transcript_path = save_result.transcript_path;
        s_snapshot.last_transcript = transcript_text;
        s_snapshot.phase = Phase::kComplete;
        s_snapshot.request_in_flight = false;
        s_snapshot.last_status_message = save_result.transcript_saved
                                             ? "Transcript saved to SD"
                                             : kSavedWithoutTranscriptStatus;
        if (save_result.transcript_saved) {
            s_snapshot.last_error_code = save_result.error_code;
            s_snapshot.last_error_message = save_result.error_message;
        } else {
            s_snapshot.last_error_code = event.snapshot.last_error_code;
            s_snapshot.last_error_message = event.snapshot.last_error_message;
        }
        s_pending_recording_id.clear();
        NotifyLocked();
        return;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_snapshot.phase == Phase::kTranscribing && !event.snapshot.request_in_flight) {
        s_snapshot.phase = s_snapshot.clip_saved ? Phase::kComplete : Phase::kFailed;
        s_snapshot.last_status_message = s_snapshot.clip_saved
                                             ? kSavedWithoutTranscriptStatus
                                             : "Transcription failed";
        s_snapshot.last_error_code = event.snapshot.last_error_code;
        s_snapshot.last_error_message = event.snapshot.last_error_message;
        s_pending_recording_id.clear();
    }
    NotifyLocked();
}

const char* PhaseName(Phase phase)
{
    switch (phase) {
        case Phase::kArmed:
            return "armed";
        case Phase::kRecording:
            return "recording";
        case Phase::kAwaitingTagSelection:
            return "awaiting_tag_selection";
        case Phase::kSaving:
            return "saving";
        case Phase::kTranscribing:
            return "transcribing";
        case Phase::kComplete:
            return "complete";
        case Phase::kFailed:
            return "failed";
        case Phase::kIdle:
        default:
            return "idle";
    }
}

}  // namespace recording_session_service
