#ifndef STORAGE_SERVICE_H_
#define STORAGE_SERVICE_H_

#include <cstdint>

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

struct StorageStats {
    bool available = false;
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    int used_percent = 0;
};

using EventHandler = void (*)(const Event& event, void* context);
using MountedFilesystemHandler = esp_err_t (*)(const char* mount_point, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
void LogDebugStatus();
bool IsMounted();
bool IsWriteBusy();
Snapshot GetSnapshot();
bool GetStorageStats(StorageStats* stats);
esp_err_t RunWithMountedFilesystem(MountedFilesystemHandler handler, void* context);

// Re-initialize the SD card after light sleep. The SDMMC card loses its state
// across a light-sleep cycle, so the cached mount handle is stale on wake and
// the first read times out (sdmmc 0x107). This forces a
// clean unmount + remount so the next SD access lands on a freshly initialized
// card. Safe to call when no card is present or storage is uninitialized.
esp_err_t RecoverAfterLightSleep();

esp_err_t RequestFormatSdCard();
const char* MountPoint();
const char* ModeName(Mode mode);
const char* OperationName(Operation operation);
const char* OperationPhaseName(OperationPhase phase);

}  // namespace storage_service

#endif  // STORAGE_SERVICE_H_
