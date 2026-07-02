#ifndef PAGE_NAVIGATION_NAVIGATION_MODEL_H_
#define PAGE_NAVIGATION_NAVIGATION_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace page_navigation {

enum class NavigationScope : uint8_t {
    kSettings = 0,
    kWifi,
    kTime,
    kDashboard,
    kVibeCheck,
    kSummarize,
    kNotes,
    kDetails,
};

enum class NavigationItemSection : uint8_t {
    kNone = 0,
    kFooter,
    kSettingsPageMenu,
    kWifiPageControls,
    kTimePageControls,
    kDashboardPageMenu,
    kVibeCheckPageControls,
    kSummarizePageControls,
    kNotesPageTimelineGroups,
    kDetailsPageControls,
};

enum class NavigationItemRole : uint8_t {
    kUnknown = 0,
    kFooterHome,
    kFooterSettings,
    kFooterWifi,
    kFooterTime,
    kSettingsWifiToggle,
    kSettingsEnableApToggle,
    kSettingsFormatSdButton,
    kWifiPageNetworkList,
    kWifiPagePasswordInput,
    kWifiPagePasswordVisibilityButton,
    kWifiPageScanButton,
    kWifiPageConnectButton,
    kTimePageTimezone,
    kTimePageHour,
    kTimePageMinute,
    kTimePageMeridiem,
    kTimePageMonth,
    kTimePageDay,
    kTimePageYear,
    kTimePageSave,
    kDashboardMenuItem,
    kVibeCheckPageCard,
    kSummarizePageSegmentControl,
    kSummarizePageScrollContainer,
    kSummarizePageGetSummaryButton,
    kNotesPageTimelineGroup,
    kDetailsPageScrollContainer,
    kDetailsPageBackButton,
};

struct NavigationItemDescriptor {
    NavigationItemSection section = NavigationItemSection::kNone;
    NavigationItemRole role = NavigationItemRole::kUnknown;
    int item_index = -1;
};

struct NavigationModel {
    NavigationScope scope = NavigationScope::kSettings;
    std::vector<NavigationItemDescriptor> items = {};
    int item_count = 0;

    const NavigationItemDescriptor* ItemAt(int index) const;
    int IndexOfRole(NavigationItemRole role) const;
    bool IsRoleSelected(int selected_index, NavigationItemRole role) const;
};

NavigationModel BuildSettingsPageNavigationModel();
NavigationModel BuildWifiPageNavigationModel();
NavigationModel BuildTimePageNavigationModel();
NavigationModel BuildDashboardPageNavigationModel();
NavigationModel BuildVibeCheckPageNavigationModel();
NavigationModel BuildSummarizePageNavigationModel();
NavigationModel BuildNotesPageNavigationModel(int timeline_group_count);
NavigationModel BuildDetailsPageNavigationModel();

}  // namespace page_navigation

#endif  // PAGE_NAVIGATION_NAVIGATION_MODEL_H_
