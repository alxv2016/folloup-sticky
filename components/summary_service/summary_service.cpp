#include "summary_service.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <utility>
#include <vector>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "followup_task_config.h"
#include "gemini_service.h"
#include "recording_archive_service.h"
#include "storage_service.h"

namespace summary_service {
namespace {

constexpr const char* kTag = "SummaryService";
constexpr int kSummaryWindowDays = 3;
constexpr int kSummaryInputTokenBudget = 120000;
constexpr int kSummaryChunkTokenBudget = 60000;
constexpr int kSummaryRollupTokenBudget = 120000;
constexpr int kMaxRollupDepth = 4;
constexpr size_t kMinSplittableChars = 1024;
constexpr UBaseType_t kSummaryQueueDepth = 4;
constexpr uint32_t kWorkerTaskStackWords = 8192;

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingMetadata;
using recording_archive_service::RecordingTag;

struct SourceEntry {
    int64_t unix_seconds = 0;
    std::string text = {};
    RecordingMetadata metadata = {};
    int part_index = 0;
    int part_count = 0;
};

struct GenerationResult {
    bool success = false;
    std::string text = {};
    CacheMetadata metadata = {};
    std::string error_code = {};
    std::string error_message = {};
};

struct QueuedRequest {
    SummaryKind kind = SummaryKind::kNone;
};

std::mutex s_mutex;
EventHandler s_event_handler = nullptr;
void* s_event_context = nullptr;
Snapshot s_snapshot = {};
QueueHandle_t s_queue = nullptr;

// --- small helpers ---------------------------------------------------------

std::string TrimCopy(std::string value)
{
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

const char* SegmentLabelForKind(SummaryKind kind)
{
    return kind == SummaryKind::kTodos ? "Todos" : "Notes";
}

// Tag -> bucket mapping mirrors recording_archive_service: Task is a Todo; everything else
// (Note, Idea) is a Note.
bool IsTodoRecordingTag(RecordingTag tag)
{
    return tag == RecordingTag::kTask;
}

bool IsNotesRecordingTag(RecordingTag tag)
{
    return !IsTodoRecordingTag(tag);
}

size_t EstimateTokenCount(const std::string& text)
{
    return std::max<size_t>(1U, text.size() / 4U);
}

// --- gemini wrappers (synchronous; run on the worker task) -----------------

size_t CountPromptTokens(const std::string& prompt)
{
    const gemini_service::TokenCountResult result = gemini_service::CountTokens(prompt);
    return result.success ? static_cast<size_t>(result.total_tokens) : EstimateTokenCount(prompt);
}

bool GeneratePromptTextResult(const std::string& prompt, std::string* text_out,
                              std::string* error_code_out, std::string* error_message_out)
{
    const gemini_service::TextResult result = gemini_service::GenerateText(prompt);
    const std::string normalized = TrimCopy(result.text);
    if (!result.success || normalized.empty()) {
        if (error_code_out != nullptr) {
            *error_code_out = result.error_code.empty() ? "summary_failed" : result.error_code;
        }
        if (error_message_out != nullptr) {
            *error_message_out =
                result.error_message.empty() ? "Gemini summary request failed" : result.error_message;
        }
        return false;
    }
    if (text_out != nullptr) {
        *text_out = normalized;
    }
    return true;
}

// --- prompt building -------------------------------------------------------

std::string BuildSummaryInstructionText(SummaryKind kind, bool intermediate)
{
    std::string text;
    if (kind == SummaryKind::kNotes) {
        text += intermediate
                    ? "Summarize the notes captured within the available transcripts. Create a "
                      "compact intermediate summary that faithfully captures the main themes, "
                      "decisions, follow-ups, open questions, and ideas. Keep it factual, plain "
                      "text, and easy to merge later. Avoid markdown tables.\n\n"
                    : "Summarize the notes captured within the available transcripts. Create a "
                      "summary capturing the main themes, decisions, follow-ups, open questions, "
                      "and ideas in plain text. Keep it concise, and write it in an encouraging "
                      "and optimistic tone so it feels insightful and motivating to look back on. "
                      "Use short paragraphs and avoid markdown tables.\n\n";
    } else {
        text += intermediate
                    ? "Summarize the todos captured within the available transcripts. Create a "
                      "compact intermediate summary that faithfully captures the priorities, "
                      "completed work, remaining tasks, and blockers, noting completion state when "
                      "it is clear from the source. Keep it factual, plain text, and easy to merge "
                      "later. Avoid markdown tables.\n\n"
                    : "Summarize the todos captured within the available transcripts. Create a "
                      "summary of the priorities, completed work, remaining tasks, and any "
                      "blockers, noting completion state when it is clear from the source. Keep it "
                      "concise and easy to skim, and write it in an encouraging and optimistic "
                      "tone that celebrates progress and motivates the next steps. Avoid markdown "
                      "tables.\n\n";
    }
    return text;
}

void AppendSourceEntriesToPrompt(std::string* prompt, const std::vector<SourceEntry>& entries)
{
    if (prompt == nullptr) {
        return;
    }
    prompt->append("Source entries:\n\n");
    for (size_t index = 0; index < entries.size(); ++index) {
        const SourceEntry& entry = entries[index];
        prompt->append("Entry ");
        prompt->append(std::to_string(index + 1));
        if (entry.part_count > 1) {
            prompt->append(" (part ");
            prompt->append(std::to_string(entry.part_index));
            prompt->append("/");
            prompt->append(std::to_string(entry.part_count));
            prompt->append(")");
        }
        prompt->append(":\n");
        if (!entry.metadata.created_local_date.empty()) {
            prompt->append("Date: ");
            prompt->append(entry.metadata.created_local_date);
            prompt->append("\n");
        }
        if (IsTodoRecordingTag(entry.metadata.tag)) {
            prompt->append("Completed: ");
            prompt->append(entry.metadata.completed ? "Yes" : "No");
            prompt->append("\n");
        }
        prompt->append("Transcript:\n");
        prompt->append(entry.text);
        prompt->append("\n\n");
    }
}

std::string BuildPromptText(SummaryKind kind, const std::vector<SourceEntry>& entries)
{
    std::string prompt;
    prompt.reserve(8192);
    prompt += BuildSummaryInstructionText(kind, false);
    AppendSourceEntriesToPrompt(&prompt, entries);
    prompt += "Now write the final summary for the ";
    prompt += SegmentLabelForKind(kind);
    prompt += ". Put the summary only in the response.";
    return prompt;
}

std::string BuildChunkSummaryPrompt(SummaryKind kind, const std::vector<SourceEntry>& entries,
                                    int chunk_index, int chunk_count)
{
    std::string prompt;
    prompt.reserve(8192);
    prompt += BuildSummaryInstructionText(kind, true);
    if (chunk_count > 1) {
        prompt += "This is chunk ";
        prompt += std::to_string(chunk_index);
        prompt += " of ";
        prompt += std::to_string(chunk_count);
        prompt += ".\n\n";
    }
    AppendSourceEntriesToPrompt(&prompt, entries);
    prompt += "Now write only the compact intermediate summary for this chunk.";
    return prompt;
}

std::string BuildRollupPrompt(SummaryKind kind, const std::vector<std::string>& partial_summaries,
                              bool intermediate)
{
    std::string prompt;
    prompt.reserve(4096);
    prompt += BuildSummaryInstructionText(kind, intermediate);
    prompt += intermediate ? "Chunk summaries to merge:\n\n" : "Intermediate summaries:\n\n";
    for (size_t index = 0; index < partial_summaries.size(); ++index) {
        prompt += intermediate ? "Chunk summary " : "Intermediate summary ";
        prompt += std::to_string(index + 1);
        prompt += ":\n";
        prompt += partial_summaries[index];
        prompt += "\n\n";
    }
    prompt += intermediate ? "Now write only one compact merged intermediate summary."
                           : "Now write the final summary only in the response.";
    return prompt;
}

// --- input splitting / chunking (map-reduce) --------------------------------

size_t FindSplitOffset(const std::string& text)
{
    if (text.size() <= 1U) {
        return 0;
    }
    const size_t midpoint = text.size() / 2U;
    const auto try_find = [&](const std::string& needle) -> size_t {
        const size_t forward = text.find(needle, midpoint);
        const size_t backward = text.rfind(needle, midpoint);
        if (forward != std::string::npos && forward > 0U && forward < text.size()) {
            return forward + needle.size();
        }
        if (backward != std::string::npos && backward > 0U && backward < text.size()) {
            return backward + needle.size();
        }
        return 0U;
    };
    for (const char* needle : {"\n\n", "\n", ". ", "; ", ", "}) {
        const size_t split = try_find(needle);
        if (split > 0U && split < text.size()) {
            return split;
        }
    }
    return midpoint;
}

bool SplitEntryToFitTokenBudget(SummaryKind kind, const SourceEntry& entry, size_t token_budget,
                                std::vector<SourceEntry>* out)
{
    if (out == nullptr) {
        return false;
    }
    std::deque<std::string> pending = {entry.text};
    std::vector<std::string> fragments;
    while (!pending.empty()) {
        const std::string text = std::move(pending.front());
        pending.pop_front();

        SourceEntry candidate = entry;
        candidate.text = text;
        if (CountPromptTokens(BuildChunkSummaryPrompt(kind, {candidate}, 1, 1)) <= token_budget) {
            fragments.push_back(std::move(candidate.text));
            continue;
        }
        if (text.size() < kMinSplittableChars) {
            return false;
        }
        const size_t split_offset = FindSplitOffset(text);
        if (split_offset == 0U || split_offset >= text.size()) {
            return false;
        }
        std::string left = TrimCopy(text.substr(0, split_offset));
        std::string right = TrimCopy(text.substr(split_offset));
        if (left.empty() || right.empty()) {
            return false;
        }
        pending.push_front(std::move(right));
        pending.push_front(std::move(left));
    }

    out->clear();
    out->reserve(fragments.size());
    for (size_t index = 0; index < fragments.size(); ++index) {
        SourceEntry fragment = entry;
        fragment.text = std::move(fragments[index]);
        fragment.part_index = static_cast<int>(index + 1U);
        fragment.part_count = static_cast<int>(fragments.size());
        out->push_back(std::move(fragment));
    }
    return !out->empty();
}

std::vector<std::vector<SourceEntry>> BuildChunkGroups(SummaryKind kind,
                                                       const std::vector<SourceEntry>& entries,
                                                       size_t token_budget, bool* success_out)
{
    bool success = true;
    std::vector<std::vector<SourceEntry>> chunks;
    std::vector<SourceEntry> current_chunk;

    for (const SourceEntry& entry : entries) {
        if (current_chunk.empty()) {
            current_chunk.push_back(entry);
            continue;
        }
        std::vector<SourceEntry> trial_chunk = current_chunk;
        trial_chunk.push_back(entry);
        if (CountPromptTokens(BuildChunkSummaryPrompt(kind, trial_chunk, 1, 1)) <= token_budget) {
            current_chunk = std::move(trial_chunk);
            continue;
        }
        chunks.push_back(std::move(current_chunk));
        current_chunk = {entry};
    }
    if (!current_chunk.empty()) {
        chunks.push_back(std::move(current_chunk));
    }

    for (const std::vector<SourceEntry>& chunk : chunks) {
        if (chunk.empty() ||
            CountPromptTokens(BuildChunkSummaryPrompt(kind, chunk, 1, 1)) > token_budget) {
            success = false;
            break;
        }
    }
    if (success_out != nullptr) {
        *success_out = success;
    }
    return chunks;
}

bool SplitSummaryTextForRollup(SummaryKind kind, const std::string& summary_text,
                               size_t token_budget, std::vector<std::string>* out)
{
    if (out == nullptr) {
        return false;
    }
    std::deque<std::string> pending = {summary_text};
    std::vector<std::string> fragments;
    while (!pending.empty()) {
        const std::string text = std::move(pending.front());
        pending.pop_front();
        if (CountPromptTokens(BuildRollupPrompt(kind, {text}, true)) <= token_budget) {
            fragments.push_back(text);
            continue;
        }
        if (text.size() < kMinSplittableChars) {
            return false;
        }
        const size_t split_offset = FindSplitOffset(text);
        if (split_offset == 0U || split_offset >= text.size()) {
            return false;
        }
        std::string left = TrimCopy(text.substr(0, split_offset));
        std::string right = TrimCopy(text.substr(split_offset));
        if (left.empty() || right.empty()) {
            return false;
        }
        pending.push_front(std::move(right));
        pending.push_front(std::move(left));
    }
    *out = std::move(fragments);
    return !out->empty();
}

bool GenerateRollupSummaryRecursive(SummaryKind kind,
                                    const std::vector<std::string>& partial_summaries, int depth,
                                    std::string* text_out, std::string* error_code_out,
                                    std::string* error_message_out)
{
    if (partial_summaries.empty()) {
        if (error_code_out != nullptr) {
            *error_code_out = "summary_empty";
        }
        if (error_message_out != nullptr) {
            *error_message_out = "No intermediate summaries are available";
        }
        return false;
    }

    const std::string prompt = BuildRollupPrompt(kind, partial_summaries, false);
    if (CountPromptTokens(prompt) <= static_cast<size_t>(kSummaryRollupTokenBudget)) {
        return GeneratePromptTextResult(prompt, text_out, error_code_out, error_message_out);
    }
    if (depth >= kMaxRollupDepth) {
        if (error_code_out != nullptr) {
            *error_code_out = "summary_rollup_too_large";
        }
        if (error_message_out != nullptr) {
            *error_message_out = "Summary rollup is still too large";
        }
        return false;
    }

    std::vector<std::string> prepared_summaries;
    for (const std::string& summary : partial_summaries) {
        std::vector<std::string> split_summaries;
        if (!SplitSummaryTextForRollup(kind, summary, kSummaryChunkTokenBudget, &split_summaries)) {
            if (error_code_out != nullptr) {
                *error_code_out = "summary_rollup_split_failed";
            }
            if (error_message_out != nullptr) {
                *error_message_out = "Summary rollup could not be split";
            }
            return false;
        }
        prepared_summaries.insert(prepared_summaries.end(), split_summaries.begin(),
                                  split_summaries.end());
    }

    std::vector<std::vector<std::string>> batches;
    std::vector<std::string> current_batch;
    for (const std::string& summary : prepared_summaries) {
        if (current_batch.empty()) {
            current_batch.push_back(summary);
            continue;
        }
        std::vector<std::string> trial_batch = current_batch;
        trial_batch.push_back(summary);
        if (CountPromptTokens(BuildRollupPrompt(kind, trial_batch, true)) <=
            static_cast<size_t>(kSummaryChunkTokenBudget)) {
            current_batch = std::move(trial_batch);
            continue;
        }
        batches.push_back(std::move(current_batch));
        current_batch = {summary};
    }
    if (!current_batch.empty()) {
        batches.push_back(std::move(current_batch));
    }

    std::vector<std::string> merged_partials;
    merged_partials.reserve(batches.size());
    for (const std::vector<std::string>& batch : batches) {
        std::string merged_text;
        if (!GeneratePromptTextResult(BuildRollupPrompt(kind, batch, true), &merged_text,
                                      error_code_out, error_message_out)) {
            return false;
        }
        merged_partials.push_back(std::move(merged_text));
    }
    return GenerateRollupSummaryRecursive(kind, merged_partials, depth + 1, text_out, error_code_out,
                                          error_message_out);
}

// --- SD cache persistence ---------------------------------------------------

std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    if (left.back() == '/') {
        return left + right;
    }
    return left + "/" + right;
}

const char* SummaryFileBase(SummaryKind kind)
{
    return kind == SummaryKind::kTodos ? "todos_latest" : "notes_latest";
}

bool ReadTextFile(const std::string& path, std::string* out)
{
    if (out == nullptr) {
        return false;
    }
    FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return false;
    }
    const long size = std::ftell(file);
    if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return false;
    }
    std::string content(static_cast<size_t>(size), '\0');
    const size_t read = size == 0 ? 0 : std::fread(content.data(), 1, content.size(), file);
    std::fclose(file);
    if (read != content.size()) {
        return false;
    }
    *out = std::move(content);
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& text)
{
    FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    const bool ok = text.empty() || std::fwrite(text.data(), 1, text.size(), file) == text.size();
    std::fclose(file);
    return ok;
}

void ParseMetadataJson(const std::string& json_text, CacheMetadata* metadata)
{
    if (metadata == nullptr) {
        return;
    }
    cJSON* root = cJSON_ParseWithLength(json_text.c_str(), json_text.size());
    if (root == nullptr) {
        return;
    }
    const auto read_int = [&](const char* key, int* out) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
        if (cJSON_IsNumber(item) && out != nullptr) {
            *out = item->valueint;
        }
    };
    const auto read_bool = [&](const char* key, bool* out) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
        if (cJSON_IsBool(item) && out != nullptr) {
            *out = cJSON_IsTrue(item);
        }
    };
    cJSON* generated = cJSON_GetObjectItemCaseSensitive(root, "generated_unix_seconds");
    if (cJSON_IsNumber(generated)) {
        metadata->generated_unix_seconds = static_cast<int64_t>(generated->valuedouble);
    }
    read_int("source_item_count", &metadata->source_item_count);
    read_int("transcript_item_count", &metadata->transcript_item_count);
    read_int("missing_transcript_item_count", &metadata->missing_transcript_item_count);
    read_int("window_days", &metadata->window_days);
    read_bool("truncated", &metadata->truncated);
    read_bool("chunked", &metadata->chunked);
    cJSON_Delete(root);
}

std::string BuildMetadataJson(const CacheMetadata& metadata, SummaryKind kind)
{
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "kind", SummaryKindName(kind));
    cJSON_AddNumberToObject(root, "generated_unix_seconds",
                            static_cast<double>(metadata.generated_unix_seconds));
    cJSON_AddNumberToObject(root, "source_item_count", metadata.source_item_count);
    cJSON_AddNumberToObject(root, "transcript_item_count", metadata.transcript_item_count);
    cJSON_AddNumberToObject(root, "missing_transcript_item_count",
                            metadata.missing_transcript_item_count);
    cJSON_AddBoolToObject(root, "truncated", metadata.truncated);
    cJSON_AddBoolToObject(root, "chunked", metadata.chunked);
    cJSON_AddNumberToObject(root, "window_days", metadata.window_days);

    char* raw = cJSON_PrintUnformatted(root);
    std::string json = raw != nullptr ? raw : "";
    if (raw != nullptr) {
        cJSON_free(raw);
    }
    cJSON_Delete(root);
    return json;
}

bool EnsureSummaryDirectory(const std::string& dir)
{
    errno = 0;
    return mkdir(dir.c_str(), 0775) == 0 || errno == EEXIST;
}

struct LoadCacheContext {
    bool storage_available = false;
    CacheEntrySnapshot notes = {};
    CacheEntrySnapshot todos = {};
};

void LoadCacheEntry(const std::string& dir, SummaryKind kind, CacheEntrySnapshot* out)
{
    const std::string base = JoinPath(dir, SummaryFileBase(kind));
    std::string text;
    if (!ReadTextFile(base + ".txt", &text) || text.empty()) {
        return;
    }
    out->available = true;
    out->text = std::move(text);
    std::string metadata_json;
    if (ReadTextFile(base + ".json", &metadata_json)) {
        ParseMetadataJson(metadata_json, &out->metadata);
    }
}

esp_err_t LoadCacheOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* ctx = static_cast<LoadCacheContext*>(context);
    if (mount_point == nullptr || ctx == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const std::string dir = JoinPath(mount_point, "summaries");
    ctx->storage_available = EnsureSummaryDirectory(dir);
    LoadCacheEntry(dir, SummaryKind::kNotes, &ctx->notes);
    LoadCacheEntry(dir, SummaryKind::kTodos, &ctx->todos);
    return ESP_OK;
}

struct SaveCacheContext {
    SummaryKind kind = SummaryKind::kNone;
    const std::string* text = nullptr;
    const std::string* metadata_json = nullptr;
    bool saved = false;
};

esp_err_t SaveCacheOnMountedFilesystem(const char* mount_point, void* context)
{
    auto* ctx = static_cast<SaveCacheContext*>(context);
    if (mount_point == nullptr || ctx == nullptr || ctx->text == nullptr ||
        ctx->metadata_json == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const std::string dir = JoinPath(mount_point, "summaries");
    if (!EnsureSummaryDirectory(dir)) {
        return ESP_FAIL;
    }
    const std::string base = JoinPath(dir, SummaryFileBase(ctx->kind));
    ctx->saved = WriteTextFile(base + ".txt", *ctx->text) &&
                 WriteTextFile(base + ".json", *ctx->metadata_json);
    return ctx->saved ? ESP_OK : ESP_FAIL;
}

// --- source gathering -------------------------------------------------------

int64_t ResolveEntryUnixSeconds(const RecordingEntry& entry)
{
    if (entry.metadata.created_unix_seconds > 0) {
        return entry.metadata.created_unix_seconds;
    }
    return entry.modified_unix_seconds;
}

std::vector<RecordingEntry> FilterWindowedEntries(const std::vector<RecordingEntry>& entries,
                                                  SummaryKind kind, int* source_item_count_out)
{
    std::vector<RecordingEntry> filtered;
    int source_item_count = 0;

    const time_t now = time(nullptr);
    const int64_t cutoff =
        now > static_cast<time_t>(7 * 24 * 60 * 60)
            ? static_cast<int64_t>(now) - static_cast<int64_t>(kSummaryWindowDays) * 24 * 60 * 60
            : 0;

    for (const RecordingEntry& entry : entries) {
        const bool matches_kind = kind == SummaryKind::kTodos
                                      ? IsTodoRecordingTag(entry.metadata.tag)
                                      : IsNotesRecordingTag(entry.metadata.tag);
        if (!matches_kind) {
            continue;
        }
        const int64_t entry_unix_seconds = ResolveEntryUnixSeconds(entry);
        if (cutoff > 0 && entry_unix_seconds > 0 && entry_unix_seconds < cutoff) {
            continue;
        }
        ++source_item_count;
        filtered.push_back(entry);
    }

    std::sort(filtered.begin(), filtered.end(),
              [](const RecordingEntry& left, const RecordingEntry& right) {
                  return ResolveEntryUnixSeconds(left) < ResolveEntryUnixSeconds(right);
              });

    if (source_item_count_out != nullptr) {
        *source_item_count_out = source_item_count;
    }
    return filtered;
}

// Gather source entries from already-transcribed recordings. Audio-only recordings (no
// transcript yet) are skipped and counted -- summarizing does NOT transcribe on the fly, since
// that means one blocking Gemini round-trip per recording (slow, and prone to 503/timeout that
// tanks the whole run). Recordings are transcribed at capture time; ideas via the Vibe Check star.
std::vector<SourceEntry> CollectSourceEntries(SummaryKind kind,
                                              const std::vector<RecordingEntry>& entries,
                                              int* transcript_count_out, int* missing_count_out)
{
    std::vector<SourceEntry> source_entries;
    int transcript_count = 0;
    int missing_count = 0;

    for (const RecordingEntry& entry : entries) {
        std::string transcript = TrimCopy(entry.transcript_text);
        if (transcript.empty()) {
            ++missing_count;
            continue;
        }
        ++transcript_count;
        source_entries.push_back({
            .unix_seconds = ResolveEntryUnixSeconds(entry),
            .text = std::move(transcript),
            .metadata = entry.metadata,
        });
    }
    (void)kind;

    if (transcript_count_out != nullptr) {
        *transcript_count_out = transcript_count;
    }
    if (missing_count_out != nullptr) {
        *missing_count_out = missing_count;
    }
    return source_entries;
}

// --- summary generation -----------------------------------------------------

GenerationResult GenerateChunkedSummary(SummaryKind kind, const std::vector<SourceEntry>& entries,
                                        CacheMetadata metadata)
{
    GenerationResult result = {};
    std::vector<SourceEntry> prepared_entries;
    for (const SourceEntry& entry : entries) {
        if (CountPromptTokens(BuildChunkSummaryPrompt(kind, {entry}, 1, 1)) <=
            static_cast<size_t>(kSummaryChunkTokenBudget)) {
            prepared_entries.push_back(entry);
            continue;
        }
        std::vector<SourceEntry> fragments;
        if (!SplitEntryToFitTokenBudget(kind, entry, kSummaryChunkTokenBudget, &fragments)) {
            result.error_code = "summary_chunk_split_failed";
            result.error_message = "Summary input could not be chunked";
            result.metadata = metadata;
            return result;
        }
        prepared_entries.insert(prepared_entries.end(), fragments.begin(), fragments.end());
    }

    bool chunks_valid = false;
    const std::vector<std::vector<SourceEntry>> chunks =
        BuildChunkGroups(kind, prepared_entries, kSummaryChunkTokenBudget, &chunks_valid);
    if (!chunks_valid || chunks.empty()) {
        result.error_code = "summary_chunk_failed";
        result.error_message = "Summary input could not be chunked";
        result.metadata = metadata;
        return result;
    }

    std::vector<std::string> partial_summaries;
    partial_summaries.reserve(chunks.size());
    for (size_t index = 0; index < chunks.size(); ++index) {
        std::string partial_summary;
        if (!GeneratePromptTextResult(
                BuildChunkSummaryPrompt(kind, chunks[index], static_cast<int>(index + 1U),
                                        static_cast<int>(chunks.size())),
                &partial_summary, &result.error_code, &result.error_message)) {
            result.metadata = metadata;
            return result;
        }
        partial_summaries.push_back(std::move(partial_summary));
    }

    std::string final_summary;
    if (!GenerateRollupSummaryRecursive(kind, partial_summaries, 0, &final_summary,
                                        &result.error_code, &result.error_message)) {
        result.metadata = metadata;
        return result;
    }

    metadata.chunked = true;
    metadata.generated_unix_seconds = static_cast<int64_t>(time(nullptr));
    result.success = true;
    result.text = std::move(final_summary);
    result.metadata = metadata;
    return result;
}

GenerationResult GenerateSummary(SummaryKind kind)
{
    GenerationResult result = {};

    ESP_LOGI(kTag, "Generating %s summary", SummaryKindName(kind));

    const gemini_service::Snapshot gemini_snapshot = gemini_service::GetSnapshot();
    if (!gemini_snapshot.runtime.ready) {
        result.error_code = "gemini_not_ready";
        result.error_message = "Gemini is not connected";
        ESP_LOGW(kTag, "Summary aborted: Gemini not connected");
        return result;
    }
    if (gemini_service::GetEffectiveApiKey().empty() ||
        gemini_service::GetEffectiveModelName().empty()) {
        result.error_code = "gemini_not_configured";
        result.error_message = "Gemini is not configured";
        ESP_LOGW(kTag, "Summary aborted: Gemini not configured");
        return result;
    }

    esp_err_t list_status = ESP_OK;
    const std::vector<RecordingEntry> recordings =
        recording_archive_service::ListRecordings(&list_status);
    if (list_status != ESP_OK) {
        // Distinct from "no recordings": the SD read failed, so we can't trust an
        // empty result. Surface it honestly instead of caching a bogus summary.
        result.error_code = "storage_read_failed";
        result.error_message = "Couldn't read recordings from SD";
        ESP_LOGW(kTag, "Summary aborted: SD read failed (%s)", esp_err_to_name(list_status));
        return result;
    }
    int source_item_count = 0;
    const std::vector<RecordingEntry> filtered_entries =
        FilterWindowedEntries(recordings, kind, &source_item_count);

    CacheMetadata metadata = {};
    metadata.window_days = kSummaryWindowDays;
    metadata.source_item_count = source_item_count;
    int transcript_count = 0;
    int missing_count = 0;
    const std::vector<SourceEntry> entries =
        CollectSourceEntries(kind, filtered_entries, &transcript_count, &missing_count);
    metadata.transcript_item_count = transcript_count;
    metadata.missing_transcript_item_count = missing_count;
    if (entries.empty()) {
        result.error_code = "no_summary_source";
        result.error_message = "No transcribed recordings are available for this summary yet";
        result.metadata = metadata;
        ESP_LOGW(kTag,
                 "Summary aborted: no transcribed source (items=%d transcribed=%d missing=%d)",
                 source_item_count, transcript_count, missing_count);
        return result;
    }

    ESP_LOGI(kTag, "Summary source ready: items=%d transcribed=%d missing=%d", source_item_count,
             transcript_count, missing_count);

    std::vector<SourceEntry> trimmed_entries = entries;
    std::string prompt = BuildPromptText(kind, trimmed_entries);
    size_t estimated_tokens = CountPromptTokens(prompt);
    while (estimated_tokens > static_cast<size_t>(kSummaryInputTokenBudget) &&
           trimmed_entries.size() > 1U) {
        metadata.truncated = true;
        trimmed_entries.erase(trimmed_entries.begin());
        prompt = BuildPromptText(kind, trimmed_entries);
        estimated_tokens = CountPromptTokens(prompt);
    }

    if (estimated_tokens > static_cast<size_t>(kSummaryInputTokenBudget)) {
        metadata.truncated = true;
        ESP_LOGI(kTag, "Summary using chunked map-reduce: tokens=%u budget=%d",
                 static_cast<unsigned>(estimated_tokens), kSummaryInputTokenBudget);
        return GenerateChunkedSummary(kind, trimmed_entries, metadata);
    }

    ESP_LOGI(kTag, "Summary single-pass request: tokens=%u", static_cast<unsigned>(estimated_tokens));

    std::string final_summary;
    if (!GeneratePromptTextResult(prompt, &final_summary, &result.error_code,
                                  &result.error_message)) {
        result.metadata = metadata;
        return result;
    }

    metadata.generated_unix_seconds = static_cast<int64_t>(time(nullptr));
    result.success = true;
    result.text = std::move(final_summary);
    result.metadata = metadata;
    return result;
}

// --- snapshot / events ------------------------------------------------------

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

bool PersistSummary(SummaryKind kind, const GenerationResult& result)
{
    const std::string metadata_json = BuildMetadataJson(result.metadata, kind);
    SaveCacheContext context = {};
    context.kind = kind;
    context.text = &result.text;
    context.metadata_json = &metadata_json;
    (void)storage_service::RunWithMountedFilesystem(SaveCacheOnMountedFilesystem, &context);
    return context.saved;
}

void CompleteSummaryRequest(SummaryKind kind, const GenerationResult& result)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    CacheEntrySnapshot* target = kind == SummaryKind::kTodos ? &s_snapshot.todos : &s_snapshot.notes;
    s_snapshot.request.in_flight = false;
    s_snapshot.request.kind = kind;
    s_snapshot.request.phase = result.success ? RequestPhase::kSucceeded : RequestPhase::kFailed;
    s_snapshot.request.status_message =
        result.success ? std::string(SegmentLabelForKind(kind)) + " summary updated"
                       : std::string("Unable to summarize ") + SegmentLabelForKind(kind);
    s_snapshot.request.error_code = result.error_code;
    s_snapshot.request.error_message = result.error_message;
    ++s_snapshot.request_generation;
    if (result.success && target != nullptr) {
        target->available = true;
        target->text = result.text;
        target->metadata = result.metadata;
    }
    if (result.success) {
        ESP_LOGI(kTag, "Summary %s succeeded: chars=%u chunked=%d", SummaryKindName(kind),
                 static_cast<unsigned>(result.text.size()), result.metadata.chunked ? 1 : 0);
    } else {
        ESP_LOGW(kTag, "Summary %s failed: code=%s message=%s", SummaryKindName(kind),
                 result.error_code.empty() ? "<none>" : result.error_code.c_str(),
                 result.error_message.empty() ? "<none>" : result.error_message.c_str());
    }
    NotifyLocked();
}

void ProcessSummaryRequest(SummaryKind kind)
{
    GenerationResult result = GenerateSummary(kind);
    if (result.success && !PersistSummary(kind, result)) {
        result.success = false;
        result.error_code = "summary_save_failed";
        result.error_message = "Unable to save summary to SD card";
    }
    CompleteSummaryRequest(kind, result);
}

void WorkerTask(void*)
{
    while (true) {
        QueuedRequest request = {};
        if (s_queue == nullptr || xQueueReceive(s_queue, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        ProcessSummaryRequest(request.kind);
    }
}

}  // namespace

esp_err_t Init()
{
    bool should_refresh = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_snapshot.initialized) {
            return ESP_OK;
        }
        if (s_queue == nullptr) {
            s_queue = xQueueCreate(kSummaryQueueDepth, sizeof(QueuedRequest));
            if (s_queue == nullptr) {
                ESP_LOGE(kTag, "Failed to create summary queue");
                return ESP_ERR_NO_MEM;
            }
            if (xTaskCreatePinnedToCore(WorkerTask, "summary_service", kWorkerTaskStackWords, nullptr,
                                        followup_task_config::kPriorityGemini, nullptr,
                                        followup_task_config::kSystemCore) != pdPASS) {
                ESP_LOGE(kTag, "Failed to start summary worker");
                vQueueDelete(s_queue);
                s_queue = nullptr;
                return ESP_ERR_NO_MEM;
            }
        }
        s_snapshot.initialized = true;
        should_refresh = true;
    }
    if (should_refresh) {
        (void)RefreshCachedSummaries();
    }
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

bool RefreshCachedSummaries()
{
    LoadCacheContext context = {};
    (void)storage_service::RunWithMountedFilesystem(LoadCacheOnMountedFilesystem, &context);
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_snapshot.storage_available = context.storage_available;
        s_snapshot.notes = std::move(context.notes);
        s_snapshot.todos = std::move(context.todos);
        NotifyLocked();
    }
    return true;
}

bool RequestSummary(SummaryKind kind)
{
    if (kind != SummaryKind::kNotes && kind != SummaryKind::kTodos) {
        return false;
    }

    QueueHandle_t queue = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_snapshot.initialized || s_queue == nullptr || s_snapshot.request.in_flight) {
            return false;
        }
        s_snapshot.request.in_flight = true;
        s_snapshot.request.kind = kind;
        s_snapshot.request.phase = RequestPhase::kStarted;
        s_snapshot.request.status_message = std::string("Summarizing ") + SegmentLabelForKind(kind);
        s_snapshot.request.error_code.clear();
        s_snapshot.request.error_message.clear();
        ++s_snapshot.request_generation;
        queue = s_queue;
        NotifyLocked();
    }

    const QueuedRequest request = {.kind = kind};
    if (xQueueSend(queue, &request, 0) == pdTRUE) {
        return true;
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    s_snapshot.request.in_flight = false;
    s_snapshot.request.phase = RequestPhase::kFailed;
    s_snapshot.request.status_message = "Unable to queue summary request";
    s_snapshot.request.error_code = "queue_full";
    s_snapshot.request.error_message = "Unable to queue summary request";
    ++s_snapshot.request_generation;
    NotifyLocked();
    return false;
}

const char* SummaryKindName(SummaryKind kind)
{
    switch (kind) {
        case SummaryKind::kNotes:
            return "notes";
        case SummaryKind::kTodos:
            return "todos";
        case SummaryKind::kNone:
        default:
            return "none";
    }
}

}  // namespace summary_service
