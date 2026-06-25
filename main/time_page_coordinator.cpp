#include "time_page_coordinator.h"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

using page_navigation::NavigationItemRole;

// Splits "a<sep>b<sep>c" into up to 3 parts; missing parts are empty.
std::array<std::string, 3> Split3(const std::string& text, char sep)
{
    std::array<std::string, 3> parts = {};
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        const size_t end = text.find(sep, start);
        parts[static_cast<size_t>(i)] =
            text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

std::string Pad2(int value)
{
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02d", value);
    return std::string(buffer);
}

}  // namespace

TimePageCoordinator::TimePageCoordinator() = default;

void TimePageCoordinator::LoadFromSnapshot(const timezone_service::Snapshot& snapshot)
{
    timezone_name_ = snapshot.settings.timezone_name;

    timezone_description_ = timezone_name_;
    for (const timezone_service::TimezoneInfo& info : timezones_) {
        if (info.name == timezone_name_) {
            timezone_description_ = info.description.empty() ? info.name : info.description;
            break;
        }
    }

    const std::array<std::string, 3> date = Split3(snapshot.runtime.current_date, '-');
    year_ = date[0];
    month_ = date[1];
    day_ = date[2];

    const std::array<std::string, 3> time = Split3(snapshot.runtime.current_time, ':');
    const int hour24 = std::atoi(time[0].c_str());
    minute_ = time[1].empty() ? std::string("00") : time[1];
    meridiem_pm_ = hour24 >= 12;
    int hour12 = hour24 % 12;
    if (hour12 == 0) {
        hour12 = 12;
    }
    hour_ = Pad2(hour12);
}

void TimePageCoordinator::RefreshFromService(
    const timezone_service::Snapshot& snapshot,
    const std::vector<timezone_service::TimezoneInfo>& timezones)
{
    timezones_ = timezones;
    if (!user_edited_) {
        LoadFromSnapshot(snapshot);
    }
}

void TimePageCoordinator::PrepareForShow()
{
    user_edited_ = false;
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kTimePageTimezone));
}

bool TimePageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    return focus_.Move(delta);
}

bool TimePageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool TimePageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

NavigationItemRole TimePageCoordinator::FocusedRole() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    return item != nullptr ? item->role : NavigationItemRole::kUnknown;
}

int TimePageCoordinator::SelectedTimezoneIndex() const
{
    for (size_t index = 0; index < timezones_.size(); ++index) {
        if (timezones_[index].name == timezone_name_) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

void TimePageCoordinator::SetTimezoneByIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(timezones_.size())) {
        return;
    }
    timezone_name_ = timezones_[static_cast<size_t>(index)].name;
    timezone_description_ = timezones_[static_cast<size_t>(index)].description.empty()
                                ? timezones_[static_cast<size_t>(index)].name
                                : timezones_[static_cast<size_t>(index)].description;
    user_edited_ = true;
}

void TimePageCoordinator::SetHour(const std::string& value)
{
    hour_ = value;
    user_edited_ = true;
}

void TimePageCoordinator::SetMinute(const std::string& value)
{
    minute_ = value;
    user_edited_ = true;
}

void TimePageCoordinator::SetMonth(const std::string& value)
{
    month_ = value;
    user_edited_ = true;
}

void TimePageCoordinator::SetDay(const std::string& value)
{
    day_ = value;
    user_edited_ = true;
}

void TimePageCoordinator::SetYear(const std::string& value)
{
    year_ = value;
    user_edited_ = true;
}

void TimePageCoordinator::ToggleMeridiem()
{
    meridiem_pm_ = !meridiem_pm_;
    user_edited_ = true;
}

void TimePageCoordinator::MarkSaved()
{
    user_edited_ = false;
}

timezone_service::SettingsPatch TimePageCoordinator::BuildSettingsPatch() const
{
    timezone_service::SettingsPatch patch = {};
    patch.has_enabled = true;
    patch.enabled = true;
    patch.has_timezone_name = true;
    patch.timezone_name = timezone_name_;

    int hour12 = std::atoi(hour_.c_str());
    int hour24 = hour12 % 12;
    if (meridiem_pm_) {
        hour24 += 12;
    }
    patch.has_manual_datetime = true;
    patch.manual_date = year_ + "-" + month_ + "-" + day_;
    patch.manual_time = Pad2(hour24) + ":" + (minute_.empty() ? std::string("00") : minute_);
    return patch;
}

epaper_ui::TimePageState TimePageCoordinator::BuildState() const
{
    epaper_ui::TimePageState state = {};
    state.navigation_focus_index = focus_.index();

    state.timezone.label_text = "Timezone";
    state.timezone.placeholder_text = "Select timezone";
    state.timezone.value_text = timezone_description_;
    state.timezone.focused = IsRoleFocused(NavigationItemRole::kTimePageTimezone);

    state.hour.value_text = hour_;
    state.hour.suffix_text = "HR";
    state.hour.max_length = 2;
    state.hour.focused = IsRoleFocused(NavigationItemRole::kTimePageHour);

    state.minute.value_text = minute_;
    state.minute.suffix_text = "MIN";
    state.minute.max_length = 2;
    state.minute.focused = IsRoleFocused(NavigationItemRole::kTimePageMinute);

    state.meridiem.label_text = meridiem_pm_ ? "PM" : "AM";
    state.meridiem.selected = IsRoleFocused(NavigationItemRole::kTimePageMeridiem);

    state.month.value_text = month_;
    state.month.placeholder_text = "MM";
    state.month.max_length = 2;
    state.month.focused = IsRoleFocused(NavigationItemRole::kTimePageMonth);

    state.day.value_text = day_;
    state.day.placeholder_text = "DD";
    state.day.max_length = 2;
    state.day.focused = IsRoleFocused(NavigationItemRole::kTimePageDay);

    state.year.value_text = year_;
    state.year.placeholder_text = "YYYY";
    state.year.max_length = 4;
    state.year.focused = IsRoleFocused(NavigationItemRole::kTimePageYear);

    state.save.label_text = "Sync & Save";
    state.save.selected = IsRoleFocused(NavigationItemRole::kTimePageSave);
    return state;
}
