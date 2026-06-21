#ifndef TRANSCRIPTION_SERVICE_H_
#define TRANSCRIPTION_SERVICE_H_

#include <cstdint>
#include <string>

#include "esp_err.h"
#include "recording_service.h"

namespace transcription_service {

struct Snapshot {
    bool initialized = false;
    bool provider_ready = false;
    bool request_in_flight = false;
    int last_http_status = 0;
    std::string last_status_message = {};
    std::string last_error_code = {};
    std::string last_error_message = {};
    std::string last_transcript = {};
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();
bool BeginTranscription(recording_service::RecordedClipPtr clip);

}  // namespace transcription_service

#endif  // TRANSCRIPTION_SERVICE_H_
