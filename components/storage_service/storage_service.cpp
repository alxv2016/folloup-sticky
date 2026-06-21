#include "storage_service.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "esp_check.h"
#include "esp_log.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "power_service.h"
#include "shared_bus_service.h"
#include "sd_card.h"
#include "sticky_board.h"
#include "sticky_board_config.h"

namespace storage_service {
namespace {

constexpr const char* kTag = "StorageService";
constexpr const char* kMountPoint = "/sdcard";
constexpr const char* kProbePath = "/sdcard/SDPROBE.TXT";
constexpr const char* kProbeText = "reTerminal Sticky SD probe\n";
constexpr const char* kVolumeLabel = "FOLLOUP";
constexpr size_t kAllocationUnitSize = 16 * 1024;
constexpr size_t kMaxFiles = 5;
constexpr size_t kMaxLoggedEntries = 5;
constexpr uint32_t kWorkerTaskStackWords = 6144;
constexpr UBaseType_t kQueueDepth = 4;

constexpr const char* kDefaultDirectories[] = {
    "recordings",
    "todos",
    "summaries",
    "files",
    "trash",
    "trash/recordings",
    "trash/todos",
};

struct QueuedRequest {
    Operation operation = Operation::kNone;
};

bool s_initialized = false;
esp_err_t s_mount_result = ESP_ERR_INVALID_STATE;
std::atomic<bool> s_write_busy = false;
QueueHandle_t s_request_queue = nullptr;
TaskHandle_t s_worker_task = nullptr;
std::mutex s_state_mutex;
std::mutex s_card_mutex;
Snapshot s_snapshot = {};
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;

class WriteBusyGuard {
public:
    WriteBusyGuard()
    {
        s_write_busy.store(true, std::memory_order_relaxed);
    }

    ~WriteBusyGuard()
    {
        s_write_busy.store(false, std::memory_order_relaxed);
    }

    WriteBusyGuard(const WriteBusyGuard&) = delete;
    WriteBusyGuard& operator=(const WriteBusyGuard&) = delete;
};

class StorageBusGuard {
public:
    StorageBusGuard() = default;

    ~StorageBusGuard()
    {
        if (err_ == ESP_OK) {
            shared_bus_service::ReleaseStorage();
        }
    }

    esp_err_t Acquire()
    {
        err_ = shared_bus_service::AcquireStorage();
        return err_;
    }

    esp_err_t err() const { return err_; }

private:
    esp_err_t err_ = ESP_FAIL;
};

SdCardPins BuildPins()
{
    SdCardPins pins = {};
    pins.host_id = STICKY_SHARED_SPI_HOST;
    pins.clk = STICKY_SHARED_SPI_CLK_PIN;
    pins.mosi = STICKY_SHARED_SPI_MOSI_PIN;
    pins.miso = STICKY_SHARED_SPI_MISO_PIN;
    pins.cs = STICKY_SD_CS_PIN;
    pins.external_spi_bus = true;
    pins.power_enable = STICKY_SD_POWER_EN_PIN;
    pins.power_active_level = 1;
    pins.card_detect = STICKY_SD_DETECT_PIN;
    pins.card_detect_active_level = 0;
    return pins;
}

SdCard& Card()
{
    static SdCard card(BuildPins(), kMountPoint);
    return card;
}

bool ReadUsbDetected()
{
    power_service::Status status = {};
    if (power_service::ReadStatus(&status) != ESP_OK || !status.power_input_valid) {
        return false;
    }
    return status.usb_detected;
}

void NotifyLocked()
{
    EventHandler handler = s_event_handler;
    void* context = s_event_context;
    Event event = {};
    event.snapshot = s_snapshot;
    if (handler != nullptr) {
        handler(event, context);
    }
}

void RefreshSnapshotStorageLocked(SdCard& card)
{
    s_snapshot.initialized = s_initialized;
    s_snapshot.inserted = card.IsCardInserted();
    s_snapshot.mounted = card.IsMounted();
    s_snapshot.usb_detected = ReadUsbDetected();
}

void SetOperationLocked(Operation operation,
                        OperationPhase phase,
                        esp_err_t error = ESP_OK)
{
    s_snapshot.operation = operation;
    s_snapshot.phase = phase;
    s_snapshot.last_error = error;
}

bool EnsureDirectoryExists(const std::string& path)
{
    if (path.empty()) {
        return false;
    }

    std::string partial_path;
    if (path.front() == '/') {
        partial_path = "/";
    }

    size_t segment_start = path.front() == '/' ? 1 : 0;
    while (segment_start <= path.size()) {
        const size_t slash = path.find('/', segment_start);
        const std::string segment =
            path.substr(segment_start,
                        slash == std::string::npos ? std::string::npos
                                                   : slash - segment_start);
        if (!segment.empty()) {
            if (partial_path.size() > 1 && partial_path.back() != '/') {
                partial_path += "/";
            }
            partial_path += segment;
            if (mkdir(partial_path.c_str(), 0775) != 0 && errno != EEXIST) {
                ESP_LOGW(kTag, "Create SD directory %s failed: errno=%d",
                         partial_path.c_str(), errno);
                return false;
            }
        }
        if (slash == std::string::npos) {
            break;
        }
        segment_start = slash + 1;
    }
    return true;
}

bool EnsureDefaultDirectories(SdCard& card)
{
    for (const char* directory : kDefaultDirectories) {
        const std::string path = card.mount_point() + "/" + directory;
        if (!EnsureDirectoryExists(path)) {
            return false;
        }
    }
    return true;
}

void LogRootDirectory(SdCard& card)
{
    std::vector<SdCardFileEntry> entries;
    const esp_err_t err = card.ListDirectoryAll(card.mount_point().c_str(), &entries);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "List root directory failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(kTag, "root directory entries=%u", static_cast<unsigned>(entries.size()));
    for (size_t i = 0; i < entries.size() && i < kMaxLoggedEntries; ++i) {
        ESP_LOGI(kTag, "entry %u: %s%s (%llu bytes)",
                 static_cast<unsigned>(i + 1),
                 entries[i].name,
                 entries[i].is_directory ? "/" : "",
                 static_cast<unsigned long long>(entries[i].size_bytes));
    }
}

void RunProbeFile(SdCard& card)
{
    WriteBusyGuard write_busy;
    const bool write_ok = card.WriteBufferToFile(
        kProbePath,
        reinterpret_cast<const uint8_t*>(kProbeText),
        std::strlen(kProbeText));
    ESP_LOGI(kTag, "probe write %s: %s", kProbePath, write_ok ? "ok" : "failed");
    if (!write_ok) {
        return;
    }

    char readback[64] = {};
    const size_t bytes_read =
        card.ReadFromOffset(kProbePath, readback, sizeof(readback) - 1, 0);
    readback[bytes_read] = '\0';
    ESP_LOGI(kTag, "probe read: %u bytes '%s'",
             static_cast<unsigned>(bytes_read), readback);
}

esp_err_t MountCardLocked(SdCard& card)
{
    if (card.IsMounted()) {
        s_mount_result = ESP_OK;
        return ESP_OK;
    }

    const bool inserted = card.IsCardInserted();
    if (!inserted) {
        s_mount_result = ESP_ERR_NOT_FOUND;
        return ESP_ERR_NOT_FOUND;
    }

    s_mount_result = sticky_board::EnsureSharedSpiBus();
    if (s_mount_result != ESP_OK) {
        ESP_LOGW(kTag, "Shared SPI bus init failed: %s", esp_err_to_name(s_mount_result));
        return s_mount_result;
    }

    s_mount_result = card.Mount(false, kAllocationUnitSize, kMaxFiles, true);
    ESP_LOGI(kTag, "Mount %s: %s", kMountPoint, esp_err_to_name(s_mount_result));
    return s_mount_result;
}

esp_err_t InitializeCardAtBootLocked(SdCard& card)
{
    const bool inserted = card.IsCardInserted();
    ESP_LOGI(kTag, "SD detect state: %s", inserted ? "inserted" : "not inserted");
    if (!inserted) {
        s_mount_result = ESP_ERR_NOT_FOUND;
        return s_mount_result;
    }

    StorageBusGuard bus_guard;
    s_mount_result = bus_guard.Acquire();
    if (s_mount_result != ESP_OK) {
        ESP_LOGW(kTag, "Startup shared storage bus acquire failed: %s",
                 esp_err_to_name(s_mount_result));
        return s_mount_result;
    }

    s_mount_result = MountCardLocked(card);
    if (s_mount_result == ESP_OK) {
        // This board behaves best when the inserted card stays mounted after
        // entering SPI mode, rather than being torn back down before display init.
        ESP_LOGI(kTag, "Startup SD init completed; card remains mounted");
    }
    return s_mount_result;
}

esp_err_t QueueRequest(Operation operation)
{
    if (s_request_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    QueuedRequest request = {};
    request.operation = operation;
    if (xQueueSend(s_request_queue, &request, 0) != pdTRUE) {
        ESP_LOGW(kTag, "Storage request queue full for operation=%s",
                 OperationName(operation));
        return ESP_ERR_TIMEOUT;
    }
    ESP_LOGI(kTag, "Queued operation=%s", OperationName(operation));
    return ESP_OK;
}

void CompleteOperation(Operation operation,
                       OperationPhase phase,
                       Mode mode,
                       esp_err_t error,
                       SdCard& card)
{
    std::lock_guard<std::mutex> state_lock(s_state_mutex);
    s_snapshot.mode = mode;
    RefreshSnapshotStorageLocked(card);
    SetOperationLocked(operation, phase, error);
    NotifyLocked();
}

void RefreshMountedSnapshotAndNotifyLocked(SdCard& card, esp_err_t error)
{
    std::lock_guard<std::mutex> state_lock(s_state_mutex);
    RefreshSnapshotStorageLocked(card);
    s_snapshot.last_error = error;
    NotifyLocked();
}

void HandleFormatRequest()
{
    WriteBusyGuard write_busy;
    StorageBusGuard bus_guard;
    SdCard& card = Card();

    {
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_snapshot.mode = Mode::kFormatting;
        SetOperationLocked(Operation::kFormatSd, OperationPhase::kStarted);
        RefreshSnapshotStorageLocked(card);
        NotifyLocked();
    }

    esp_err_t err = bus_guard.Acquire();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Shared storage bus acquire failed: %s", esp_err_to_name(err));
        CompleteOperation(Operation::kFormatSd, OperationPhase::kFailed,
                          Mode::kError, err, card);
        return;
    }

    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        err = MountCardLocked(card);
        if (err == ESP_OK) {
            err = card.Format();
        }
        if (err == ESP_OK) {
            const esp_err_t label_err = card.SetVolumeLabel(kVolumeLabel);
            if (label_err == ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(kTag, "SD volume label skipped: FATFS label support disabled");
            } else {
                err = label_err;
            }
        }
        if (err == ESP_OK && !EnsureDefaultDirectories(card)) {
            err = ESP_FAIL;
        }
    }

    if (err == ESP_OK) {
        ESP_LOGI(kTag, "SD card formatted: label=%s default directories recreated",
                 kVolumeLabel);
        CompleteOperation(Operation::kFormatSd, OperationPhase::kSucceeded,
                          Mode::kAppMounted, ESP_OK, card);
    } else {
        ESP_LOGW(kTag, "SD format failed: %s", esp_err_to_name(err));
        CompleteOperation(Operation::kFormatSd, OperationPhase::kFailed,
                          card.IsMounted() ? Mode::kAppMounted : Mode::kError,
                          err, card);
    }
}

void StorageWorker(void*)
{
    QueuedRequest request = {};
    while (true) {
        if (xQueueReceive(s_request_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (request.operation) {
            case Operation::kFormatSd:
                HandleFormatRequest();
                break;
            case Operation::kNone:
                break;
        }
    }
}

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return s_mount_result == ESP_ERR_NOT_FOUND ? ESP_OK : s_mount_result;
    }

    ESP_RETURN_ON_ERROR(shared_bus_service::Init(), kTag, "shared bus init failed");

    if (s_request_queue == nullptr) {
        s_request_queue = xQueueCreate(kQueueDepth, sizeof(QueuedRequest));
        if (s_request_queue == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (s_worker_task == nullptr) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            StorageWorker,
            "storage_service",
            kWorkerTaskStackWords,
            nullptr,
            followup_task_config::kPriorityStorage,
            &s_worker_task,
            followup_task_config::kAppCore);
        if (created != pdPASS) {
            s_worker_task = nullptr;
            return ESP_ERR_NO_MEM;
        }
    }

    SdCard& card = Card();
    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        s_mount_result = InitializeCardAtBootLocked(card);
    }

    {
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_initialized = true;
        s_snapshot.initialized = true;
        s_snapshot.mode = Mode::kAppMounted;
        SetOperationLocked(Operation::kNone, OperationPhase::kIdle, s_mount_result);
        RefreshSnapshotStorageLocked(card);
        NotifyLocked();
    }

    return s_mount_result == ESP_ERR_NOT_FOUND ? ESP_OK : s_mount_result;
}

void SetEventHandler(EventHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_event_handler = handler;
    s_event_context = context;
}

void LogDebugStatus()
{
    SdCard& card = Card();
    std::lock_guard<std::mutex> card_lock(s_card_mutex);
    {
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        RefreshSnapshotStorageLocked(card);
        ESP_LOGI(kTag,
                 "status: initialized=%d inserted=%d mounted=%d usb_detected=%d "
                 "mode=%s operation=%s phase=%s mount_result=%s last_error=%s",
                 s_snapshot.initialized ? 1 : 0,
                 s_snapshot.inserted ? 1 : 0,
                 s_snapshot.mounted ? 1 : 0,
                 s_snapshot.usb_detected ? 1 : 0,
                 ModeName(s_snapshot.mode),
                 OperationName(s_snapshot.operation),
                 OperationPhaseName(s_snapshot.phase),
                 esp_err_to_name(s_mount_result),
                 esp_err_to_name(s_snapshot.last_error));
    }

    if (!card.IsMounted()) {
        return;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    if (card.GetStorageStats(total_bytes, free_bytes)) {
        ESP_LOGI(kTag, "storage: total=%llu bytes free=%llu bytes",
                 static_cast<unsigned long long>(total_bytes),
                 static_cast<unsigned long long>(free_bytes));
    } else {
        ESP_LOGW(kTag, "Storage stats unavailable");
    }

    LogRootDirectory(card);
    RunProbeFile(card);
}

bool IsMounted()
{
    std::lock_guard<std::mutex> card_lock(s_card_mutex);
    return Card().IsMounted();
}

bool IsWriteBusy()
{
    return s_write_busy.load(std::memory_order_relaxed);
}

Snapshot GetSnapshot()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_snapshot;
}

esp_err_t RunWithMountedFilesystem(MountedFilesystemHandler handler, void* context)
{
    if (handler == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    WriteBusyGuard write_busy;
    StorageBusGuard bus_guard;
    const esp_err_t bus_err = bus_guard.Acquire();
    if (bus_err != ESP_OK) {
        return bus_err;
    }

    SdCard& card = Card();
    std::lock_guard<std::mutex> card_lock(s_card_mutex);

    esp_err_t err = MountCardLocked(card);
    if (err == ESP_OK) {
        err = handler(card.mount_point().c_str(), context);
    }

    RefreshMountedSnapshotAndNotifyLocked(card, err);
    return err;
}

esp_err_t RequestFormatSdCard()
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (s_snapshot.mode != Mode::kAppMounted) {
            ESP_LOGW(kTag, "Format ignored while mode=%s", ModeName(s_snapshot.mode));
            return ESP_ERR_INVALID_STATE;
        }
        s_snapshot.usb_detected = ReadUsbDetected();
        s_snapshot.mode = Mode::kFormatting;
        SetOperationLocked(Operation::kFormatSd, OperationPhase::kStarted);
        NotifyLocked();
    }
    const esp_err_t err = QueueRequest(Operation::kFormatSd);
    if (err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        s_snapshot.mode = Mode::kAppMounted;
        SetOperationLocked(Operation::kFormatSd, OperationPhase::kFailed, err);
        NotifyLocked();
    }
    return err;
}

const char* MountPoint()
{
    return kMountPoint;
}

const char* ModeName(Mode mode)
{
    switch (mode) {
        case Mode::kAppMounted:
            return "app_mounted";
        case Mode::kFormatting:
            return "formatting";
        case Mode::kError:
            return "error";
    }
    return "unknown";
}

const char* OperationName(Operation operation)
{
    switch (operation) {
        case Operation::kNone:
            return "none";
        case Operation::kFormatSd:
            return "format_sd";
    }
    return "unknown";
}

const char* OperationPhaseName(OperationPhase phase)
{
    switch (phase) {
        case OperationPhase::kIdle:
            return "idle";
        case OperationPhase::kStarted:
            return "started";
        case OperationPhase::kSucceeded:
            return "succeeded";
        case OperationPhase::kFailed:
            return "failed";
    }
    return "unknown";
}

}  // namespace storage_service
