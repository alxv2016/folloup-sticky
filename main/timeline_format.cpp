#include "timeline_format.h"

#include <cstdio>
#include <ctime>

#include "timezone_service.h"

namespace timeline_format {

using recording_archive_service::RecordingTag;

std::string DateKey(const std::string& created_local_date)
{
    const auto space = created_local_date.find(' ');
    return space == std::string::npos ? created_local_date : created_local_date.substr(0, space);
}

std::string FormatDateLabel(const std::string& created_local_date)
{
    const std::string day_key = DateKey(created_local_date);

    // "Today" only while the recording's day equals the current local date. GetSnapshot().current_date
    // is the live "YYYY-MM-DD", so this stops reading "Today" as soon as the date rolls over.
    if (!day_key.empty() &&
        day_key == timezone_service::GetSnapshot().runtime.current_date) {
        return "Today";
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (std::sscanf(day_key.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
        std::tm tm = {};
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        std::time_t stamp = std::mktime(&tm);
        if (stamp != static_cast<std::time_t>(-1)) {
            std::tm local = {};
            localtime_r(&stamp, &local);
            char buffer[24] = {};
            if (std::strftime(buffer, sizeof(buffer), "%a %b %d", &local) > 0) {
                return buffer;
            }
        }
    }
    return created_local_date.empty() ? "Today" : created_local_date;
}

std::string FormatTimeLabel(bool time_valid, int64_t created_unix_seconds)
{
    if (time_valid && created_unix_seconds > 0) {
        std::time_t stamp = static_cast<std::time_t>(created_unix_seconds);
        std::tm local = {};
        localtime_r(&stamp, &local);
        char buffer[16] = {};
        if (std::strftime(buffer, sizeof(buffer), "%I:%M %p", &local) > 0) {
            std::string text = buffer;
            if (text.size() > 1 && text.front() == '0') {
                text.erase(0, 1);
            }
            return text;
        }
    }
    return "--:--";
}

std::string FormatDurationLabel(uint32_t duration_ms)
{
    const uint32_t seconds = duration_ms / 1000U;
    if (seconds < 60U) {
        return std::to_string(seconds) + "s";
    }
    return std::to_string(seconds / 60U) + "m";
}

std::string TrimTranscript(const std::string& text)
{
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string TagText(RecordingTag tag)
{
    switch (tag) {
        case RecordingTag::kTask:
            return "Task";
        case RecordingTag::kIdea:
            return "Idea";
        case RecordingTag::kNote:
        default:
            return "Note";
    }
}

}  // namespace timeline_format
