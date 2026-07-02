#ifndef SUMMARY_SERVICE_H_
#define SUMMARY_SERVICE_H_

#include <cstdint>
#include <string>

#include "esp_err.h"

namespace summary_service {

enum class SummaryKind : uint8_t {
    kNone = 0,
    kNotes,
    kTodos,
};

enum class RequestPhase : uint8_t {
    kIdle = 0,
    kStarted,
    kSucceeded,
    kFailed,
};

// Provenance of a cached summary: how many recordings fed it, how many had transcripts, and
// whether the input had to be truncated or chunked to fit the model budget.
struct CacheMetadata {
    int64_t generated_unix_seconds = 0;
    int source_item_count = 0;
    int transcript_item_count = 0;
    int missing_transcript_item_count = 0;
    bool truncated = false;
    bool chunked = false;
    int window_days = 0;
};

struct CacheEntrySnapshot {
    bool available = false;
    std::string text = {};
    CacheMetadata metadata = {};
};

struct RequestSnapshot {
    bool in_flight = false;
    SummaryKind kind = SummaryKind::kNone;
    RequestPhase phase = RequestPhase::kIdle;
    std::string status_message = {};
    std::string error_code = {};
    std::string error_message = {};
};

struct Snapshot {
    bool initialized = false;
    bool storage_available = false;
    CacheEntrySnapshot notes = {};
    CacheEntrySnapshot todos = {};
    RequestSnapshot request = {};
    uint32_t request_generation = 0;
};

struct Event {
    Snapshot snapshot = {};
};

using EventHandler = void (*)(const Event& event, void* context);

// Creates the worker task + request queue and seeds the snapshot from any cached summaries on
// the SD card. Safe to call once at startup.
esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);
Snapshot GetSnapshot();

// Re-read the persisted summaries from SD into the snapshot (runs SD I/O on the caller's task).
bool RefreshCachedSummaries();
// Queue an async summary generation for Notes or Todos. Returns false if it can't be queued
// (not initialized, a request already in flight, or queue full). Progress is reported via events.
bool RequestSummary(SummaryKind kind);

const char* SummaryKindName(SummaryKind kind);

}  // namespace summary_service

#endif  // SUMMARY_SERVICE_H_
