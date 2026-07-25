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
#include "sd_card.h"
#include "waveshare_board_config.h"
#include "esp_timer.h"

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

SdCardPins BuildPins()
{
    SdCardPins pins = {};
    pins.slot = SDMMC_HOST_SLOT_1;
    pins.bus_width = 4;
    pins.clk = WAVESHARE_SD_CLK_PIN;
    pins.cmd = WAVESHARE_SD_CMD_PIN;
    pins.d0 = WAVESHARE_SD_D0_PIN;
    pins.d1 = WAVESHARE_SD_D1_PIN;
    pins.d2 = WAVESHARE_SD_D2_PIN;
    pins.d3 = WAVESHARE_SD_D3_PIN;
    pins.internal_pullup = true;
    // Waveshare has no card-detect or power-enable line for the SD slot.
    pins.power_enable = GPIO_NUM_NC;
    pins.card_detect = GPIO_NUM_NC;
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

Event BuildEventLocked()
{
    Event event = {};
    event.snapshot = s_snapshot;
    return event;
}

void DispatchEvent(const Event& event, EventHandler handler, void* context)
{
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

void CaptureNotificationLocked(Event* event, EventHandler* handler, void** context)
{
    if (event != nullptr) {
        *event = BuildEventLocked();
    }
    if (handler != nullptr) {
        *handler = s_event_handler;
    }
    if (context != nullptr) {
        *context = s_event_context;
    }
}

void LogFormatStep(const char* step, SdCard& card)
{
    ESP_LOGI(kTag,
             "Format step: %s inserted=%d mounted=%d mode=%s phase=%s",
             step != nullptr ? step : "<null>",
             card.IsCardInserted() ? 1 : 0,
             card.IsMounted() ? 1 : 0,
             ModeName(s_snapshot.mode),
             OperationPhaseName(s_snapshot.phase));
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
    Event event = {};
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_snapshot.mode = mode;
        RefreshSnapshotStorageLocked(card);
        SetOperationLocked(operation, phase, error);
        event = BuildEventLocked();
        handler = s_event_handler;
        context = s_event_context;
    }
    DispatchEvent(event, handler, context);
}

void RefreshMountedSnapshotAndNotify(SdCard& card, esp_err_t error)
{
    Event event = {};
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_snapshot.mode = card.IsMounted() ? Mode::kAppMounted : Mode::kError;
        RefreshSnapshotStorageLocked(card);
        // This is a storage-status refresh (mount / post-op / light-sleep recovery), not an
        // operation result. Clear any prior operation so a completed op (e.g. a format's
        // kSucceeded) is broadcast once by CompleteOperation and NOT re-fired on every later SD
        // read -- otherwise consumers re-run one-shot side effects (the format-success modal
        // re-popped on each page's ListRecordings).
        SetOperationLocked(Operation::kNone, OperationPhase::kIdle, error);
        CaptureNotificationLocked(&event, &handler, &context);
    }
    DispatchEvent(event, handler, context);
}

bool ShouldRetryMountedFilesystemOperation(esp_err_t err, const SdCard& card)
{
    if (err == ESP_OK) {
        return false;
    }

    if (!card.IsCardInserted()) {
        return false;
    }

    return err == ESP_FAIL || err == ESP_ERR_INVALID_STATE || err == ESP_ERR_TIMEOUT;
}

void HandleFormatRequest()
{
    WriteBusyGuard write_busy;
    SdCard& card = Card();
    const int64_t started_at_us = esp_timer_get_time();
    Event event = {};
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_snapshot.mode = Mode::kFormatting;
        SetOperationLocked(Operation::kFormatSd, OperationPhase::kStarted, ESP_OK);
        RefreshSnapshotStorageLocked(card);
        LogFormatStep("publish_started", card);
        CaptureNotificationLocked(&event, &handler, &context);
    }
    DispatchEvent(event, handler, context);

    esp_err_t err = ESP_OK;

    {
        std::lock_guard<std::mutex> card_lock(s_card_mutex);
        LogFormatStep("mount_before", card);
        err = MountCardLocked(card);
        ESP_LOGI(kTag,
                 "Format step: mount_after err=%s inserted=%d mounted=%d elapsed_ms=%lld",
                 esp_err_to_name(err),
                 card.IsCardInserted() ? 1 : 0,
                 card.IsMounted() ? 1 : 0,
                 static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
    }
    if (err == ESP_OK) {
        if (err == ESP_OK) {
            std::lock_guard<std::mutex> card_lock(s_card_mutex);
            LogFormatStep("format_before", card);
            err = card.Format();
            ESP_LOGI(kTag,
                     "Format step: format_after err=%s inserted=%d mounted=%d elapsed_ms=%lld",
                     esp_err_to_name(err),
                     card.IsCardInserted() ? 1 : 0,
                     card.IsMounted() ? 1 : 0,
                     static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
        }
    }
    if (err == ESP_OK) {
        if (err == ESP_OK) {
            std::lock_guard<std::mutex> card_lock(s_card_mutex);
            LogFormatStep("set_label_before", card);
            const esp_err_t label_err = card.SetVolumeLabel(kVolumeLabel);
            if (label_err == ESP_ERR_NOT_SUPPORTED) {
                ESP_LOGW(kTag, "SD volume label skipped: FATFS label support disabled");
            } else {
                err = label_err;
            }
            ESP_LOGI(kTag,
                     "Format step: set_label_after err=%s elapsed_ms=%lld",
                     esp_err_to_name(err),
                     static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
        }
    }
    if (err == ESP_OK) {
        if (err == ESP_OK) {
            std::lock_guard<std::mutex> card_lock(s_card_mutex);
            LogFormatStep("ensure_dirs_before", card);
            if (!EnsureDefaultDirectories(card)) {
                err = ESP_FAIL;
            }
            ESP_LOGI(kTag,
                     "Format step: ensure_dirs_after err=%s elapsed_ms=%lld",
                     esp_err_to_name(err),
                     static_cast<long long>((esp_timer_get_time() - started_at_us) / 1000));
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

    Event init_event = {};
    EventHandler init_handler = nullptr;
    void* init_context = nullptr;
    {
        std::lock_guard<std::mutex> state_lock(s_state_mutex);
        s_initialized = true;
        s_snapshot.initialized = true;
        s_snapshot.mode = card.IsMounted() ? Mode::kAppMounted : Mode::kError;
        SetOperationLocked(Operation::kNone, OperationPhase::kIdle, s_mount_result);
        RefreshSnapshotStorageLocked(card);
        init_event = BuildEventLocked();
        init_handler = s_event_handler;
        init_context = s_event_context;
    }
    DispatchEvent(init_event, init_handler, init_context);

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

bool GetStorageStats(StorageStats* stats)
{
    if (stats == nullptr) {
        return false;
    }

    *stats = {};
    std::lock_guard<std::mutex> card_lock(s_card_mutex);
    SdCard& card = Card();
    if (!card.IsCardInserted() || !card.IsMounted()) {
        return false;
    }

    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    if (!card.GetStorageStats(total_bytes, free_bytes) || total_bytes == 0) {
        return false;
    }

    const uint64_t clamped_free_bytes = std::min(total_bytes, free_bytes);
    const uint64_t used_bytes = total_bytes - clamped_free_bytes;
    stats->available = true;
    stats->total_bytes = total_bytes;
    stats->free_bytes = clamped_free_bytes;
    stats->used_percent = static_cast<int>((used_bytes * 100ULL) / total_bytes);
    return true;
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
    SdCard& card = Card();
    esp_err_t err = ESP_OK;
    {
        {
            std::lock_guard<std::mutex> card_lock(s_card_mutex);
            err = MountCardLocked(card);
            if (err == ESP_OK) {
                err = handler(card.mount_point().c_str(), context);
                if (ShouldRetryMountedFilesystemOperation(err, card)) {
                    ESP_LOGW(kTag,
                             "Mounted filesystem operation failed: %s; remounting SD and retrying once",
                             esp_err_to_name(err));
                    card.Unmount();
                    err = MountCardLocked(card);
                    if (err == ESP_OK) {
                        err = handler(card.mount_point().c_str(), context);
                    } else {
                        ESP_LOGW(kTag, "SD remount before retry failed: %s", esp_err_to_name(err));
                    }
                }
            }
        }
    }

    RefreshMountedSnapshotAndNotify(card, err);
    return err;
}

esp_err_t RecoverAfterLightSleep()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    SdCard& card = Card();
    if (!card.IsCardInserted()) {
        ESP_LOGI(kTag, "Light-sleep storage recovery skipped: no card inserted");
        RefreshMountedSnapshotAndNotify(card, ESP_ERR_NOT_FOUND);
        return ESP_OK;
    }

    esp_err_t err = ESP_OK;
    {
        // The SDMMC card loses its state across light sleep, so the cached mount
        // handle is stale on wake and the first read times out (sdmmc 0x107).
        // Force a clean unmount + remount to re-run the card's init sequence.
        {
            std::lock_guard<std::mutex> card_lock(s_card_mutex);
            card.Unmount();
            err = MountCardLocked(card);
            if (err == ESP_OK) {
                ESP_LOGI(kTag, "SD remounted after light sleep");
            } else {
                ESP_LOGW(kTag, "SD remount after light sleep failed: %s", esp_err_to_name(err));
            }
        }
    }

    RefreshMountedSnapshotAndNotify(card, err);
    return err;
}

esp_err_t RequestFormatSdCard()
{
    Event event = {};
    EventHandler handler = nullptr;
    void* context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (s_snapshot.mode != Mode::kAppMounted) {
            ESP_LOGW(kTag, "Format ignored while mode=%s", ModeName(s_snapshot.mode));
            return ESP_ERR_INVALID_STATE;
        }
        s_snapshot.usb_detected = ReadUsbDetected();
        s_snapshot.mode = Mode::kFormatting;
        SetOperationLocked(Operation::kFormatSd, OperationPhase::kStarted, ESP_OK);
        event = BuildEventLocked();
        handler = s_event_handler;
        context = s_event_context;
    }
    DispatchEvent(event, handler, context);
    const esp_err_t err = QueueRequest(Operation::kFormatSd);
    if (err != ESP_OK) {
        Event failure_event = {};
        EventHandler failure_handler = nullptr;
        void* failure_context = nullptr;
        {
            std::lock_guard<std::mutex> lock(s_state_mutex);
            s_snapshot.mode = Mode::kAppMounted;
            SetOperationLocked(Operation::kFormatSd, OperationPhase::kFailed, err);
            failure_event = BuildEventLocked();
            failure_handler = s_event_handler;
            failure_context = s_event_context;
        }
        DispatchEvent(failure_event, failure_handler, failure_context);
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
