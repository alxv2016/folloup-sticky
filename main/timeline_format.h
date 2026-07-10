#ifndef TIMELINE_FORMAT_H_
#define TIMELINE_FORMAT_H_

#include <string>

// Shared date helpers for the recording timeline shown on the Notes, Todos, and Follow-up pages.
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

}  // namespace timeline_format

#endif  // TIMELINE_FORMAT_H_
