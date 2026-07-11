#ifndef TIMELINE_FORMAT_H_
#define TIMELINE_FORMAT_H_

#include <cstdint>
#include <string>

#include "recording_archive_service.h"

// Shared formatters for the recording timeline shown on the Notes, Todos, and Follow-up pages (and
// the sticky-note overlay, which reuses the same content shape).
namespace timeline_format {

// Extract the "YYYY-MM-DD" day key from a created_local_date that may be either "YYYY-MM-DD" or
// "YYYY-MM-DD HH:MM:SS", so recordings from the same day group under one date chip. This is the
// stable grouping identity -- it never changes as the calendar rolls over.
std::string DateKey(const std::string& created_local_date);

// Human label for a recording's day: "Today" while its day matches the current local date,
// otherwise the absolute "%a %b %d" (e.g. "Fri Jul 10"). Because the comparison is against the
// live current date, a recording labelled "Today" automatically re-labels to its absolute date
// once midnight passes. Empty / unparseable dates fall back to "Today".
std::string FormatDateLabel(const std::string& created_local_date);

// Clock time for a recording's header: "%I:%M %p" (leading zero stripped) when the timestamp is
// valid, otherwise "--:--".
std::string FormatTimeLabel(bool time_valid, int64_t created_unix_seconds);

// Compact duration: "<Ns>" under a minute, otherwise "<Nm>".
std::string FormatDurationLabel(uint32_t duration_ms);

// Trim leading/trailing whitespace from a transcript (empty when all whitespace).
std::string TrimTranscript(const std::string& text);

// Human tag label: kTask -> "Task", kIdea -> "Idea", otherwise "Note".
std::string TagText(recording_archive_service::RecordingTag tag);

}  // namespace timeline_format

#endif  // TIMELINE_FORMAT_H_
