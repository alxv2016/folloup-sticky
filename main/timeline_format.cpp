#include "timeline_format.h"

#include <cstdio>
#include <ctime>

#include "timezone_service.h"

namespace timeline_format {

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

}  // namespace timeline_format
