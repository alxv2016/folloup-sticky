#ifndef STORAGE_SERVICE_H_
#define STORAGE_SERVICE_H_

#include "esp_err.h"

namespace storage_service {

enum class Mode {
    kAppMounted,
    kFormatting,
    kError,
};

enum class Operation {
    kNone,
    kFormatSd,
};

enum class OperationPhase {
    kIdle,
    kStarted,
    kSucceeded,
    kFailed,
};

struct Snapshot {
    bool initialized = false;
    bool inserted = false;
    bool mounted = false;
    bool usb_detected = false;
    Mode mode = Mode::kAppMounted;
    Operation operation = Operation::kNone;
    OperationPhase phase = OperationPhase::kIdle;
    esp_err_t last_error = ESP_OK;
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
void LogDebugStatus();
bool IsMounted();
bool IsWriteBusy();
Snapshot GetSnapshot();
esp_err_t RequestFormatSdCard();
const char* MountPoint();
const char* ModeName(Mode mode);
const char* OperationName(Operation operation);
const char* OperationPhaseName(OperationPhase phase);

}  // namespace storage_service

#endif  // STORAGE_SERVICE_H_
