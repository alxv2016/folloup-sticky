#ifndef DASHBOARD_PAGE_COORDINATOR_H_
#define DASHBOARD_PAGE_COORDINATOR_H_

#include "epaper_ui/dashboard_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

class DashboardPageCoordinator {
public:
    DashboardPageCoordinator();

    void RefreshFromArchive(const recording_archive_service::Snapshot& snapshot);
    // Called when the page is (re)entered: focuses the first menu item.
    void PrepareForShow();

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    page_navigation::NavigationItemRole FocusedRole() const;
    // Index of the focused menu item (0..count-1), or -1 when focus is on the footer.
    int FocusedMenuIndex() const;

    epaper_ui::DashboardPageState BuildState() const;

    // Number of rotation intervals elapsed since the epoch (0 before the clock is valid).
    // The interval length is CONFIG_FOLLOWUP_WELCOME_MESSAGE_ROTATE_HOURS. Used both to pick
    // the current greeting and to detect when it should roll over.
    static uint32_t WelcomePeriodsSinceEpoch();

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildDashboardPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    recording_archive_service::Snapshot archive_ = {};
    // Random welcome-message phase, chosen once per boot; the day count is added on top so
    // the greeting also rotates daily (see BuildState).
    uint32_t welcome_seed_ = 0;
    bool welcome_seeded_ = false;
};

#endif  // DASHBOARD_PAGE_COORDINATOR_H_
