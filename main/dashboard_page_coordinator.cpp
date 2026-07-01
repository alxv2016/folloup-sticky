#include "dashboard_page_coordinator.h"

#include <ctime>
#include <string>

#include "esp_random.h"
#include "sdkconfig.h"

namespace {

using page_navigation::NavigationItemRole;
using page_navigation::NavigationItemSection;

constexpr int64_t kMinValidEpoch = 1704067200;  // 2024-01-01 UTC

// Fills weekday/date strings from the system clock; leaves them empty when time is invalid.
void FillCurrentDate(epaper_ui::CurrentDateState* date)
{
    const time_t now = time(nullptr);
    if (static_cast<int64_t>(now) < kMinValidEpoch) {
        return;
    }
    std::tm local_tm = {};
    localtime_r(&now, &local_tm);

    char weekday[16] = {};
    char date_text[24] = {};
    std::strftime(weekday, sizeof(weekday), "%A", &local_tm);
    std::strftime(date_text, sizeof(date_text), "%b %d, %Y", &local_tm);
    date->weekday_text = weekday;
    date->date_text = date_text;
}

}  // namespace

DashboardPageCoordinator::DashboardPageCoordinator() = default;

uint32_t DashboardPageCoordinator::WelcomePeriodsSinceEpoch()
{
    const time_t now = time(nullptr);
    if (static_cast<int64_t>(now) < kMinValidEpoch) {
        return 0;
    }
    constexpr int64_t kSecondsPerHour = 3600;
    const int64_t period_seconds =
        static_cast<int64_t>(CONFIG_FOLLOWUP_WELCOME_MESSAGE_ROTATE_HOURS) * kSecondsPerHour;
    return static_cast<uint32_t>(now / period_seconds);
}

void DashboardPageCoordinator::RefreshFromArchive(
    const recording_archive_service::Snapshot& snapshot)
{
    archive_ = snapshot;
}

void DashboardPageCoordinator::PrepareForShow()
{
    if (!welcome_seeded_) {
        welcome_seed_ =
            esp_random() % static_cast<uint32_t>(epaper_ui::WelcomeMessageTitleCount());
        welcome_seeded_ = true;
    }
    focus_.Configure(navigation_model_.item_count, 0);
}

bool DashboardPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    return focus_.Move(delta);
}

bool DashboardPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool DashboardPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

NavigationItemRole DashboardPageCoordinator::FocusedRole() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    return item != nullptr ? item->role : NavigationItemRole::kUnknown;
}

int DashboardPageCoordinator::FocusedMenuIndex() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    if (item == nullptr || item->section != NavigationItemSection::kDashboardPageMenu) {
        return -1;
    }
    return item->item_index;
}

epaper_ui::DashboardPageState DashboardPageCoordinator::BuildState() const
{
    epaper_ui::DashboardPageState state = {};
    state.navigation_focus_index = focus_.index();

    FillCurrentDate(&state.welcome_message.current_date);
    // Random per-boot phase, advanced one step per configured interval so it rotates over time.
    state.welcome_message.title_text =
        epaper_ui::WelcomeMessageTitle(welcome_seed_ + WelcomePeriodsSinceEpoch());

    // Empty archive: invite the first capture. Otherwise show the task tracker.
    if (archive_.recording_count == 0) {
        state.shows_completion_banner = true;
        state.completion_banner.icon = EmbeddedIconId::kTaskStart;
        state.completion_banner.message_text = "Capture your first note with the mic";
    } else {
        state.shows_completion_banner = false;
        state.current_progress.label_text = "Task tracker";
        const int total = archive_.todo_recording_count;
        const int done = archive_.completed_todo_count;
        if (total > 0) {
            state.current_progress.status_text =
                std::to_string(done) + "/" + std::to_string(total) + " completed";
            state.current_progress.progress_percent = (done * 100) / total;
        } else {
            state.current_progress.status_text = "No tasks yet";
            state.current_progress.progress_percent = 0;
        }
    }

    state.menu.selected_index = FocusedMenuIndex();
    state.menu.shows_follow_up_badge = archive_.follow_up_recording_count > 0;
    state.menu.shows_notes_badge = archive_.notes_recording_count > 0;
    state.menu.shows_todos_badge = archive_.todo_recording_count > 0;
    state.menu.follow_up_badge_text = std::to_string(archive_.follow_up_recording_count);
    state.menu.notes_badge_text = std::to_string(archive_.notes_recording_count);
    state.menu.todos_badge_text = std::to_string(archive_.todo_recording_count);
    return state;
}
