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

SaveResult SaveClip(const recording_service::RecordedClip& clip,
                    const SaveOptions& options = {});
SaveResult SaveTranscript(const std::string& recording_id, const std::string& transcript);

const char* TagName(RecordingTag tag);

}  // namespace recording_archive_service

#endif  // RECORDING_ARCHIVE_SERVICE_H_
