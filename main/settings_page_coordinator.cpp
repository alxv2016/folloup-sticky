#include "settings_page_coordinator.h"

#include <cstdio>
#include <string>

namespace {

std::string FormatStorageBytes(uint64_t bytes)
{
    constexpr uint64_t kKilobyte = 1024ULL;
    constexpr uint64_t kMegabyte = 1024ULL * kKilobyte;
    constexpr uint64_t kGigabyte = 1024ULL * kMegabyte;

    char buffer[32] = {};
    if (bytes >= kGigabyte) {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%.1f GB free",
                      static_cast<double>(bytes) / static_cast<double>(kGigabyte));
    } else if (bytes >= kMegabyte) {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%.1f MB free",
                      static_cast<double>(bytes) / static_cast<double>(kMegabyte));
    } else if (bytes >= kKilobyte) {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%.1f KB free",
                      static_cast<double>(bytes) / static_cast<double>(kKilobyte));
    } else {
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%llu B free",
                      static_cast<unsigned long long>(bytes));
    }
    return std::string(buffer);
}

}  // namespace

SettingsPageCoordinator::SettingsPageCoordinator() = default;

void SettingsPageCoordinator::Show()
{
    focus_.Configure(navigation_model_.item_count, 0);
}

bool SettingsPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsPageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::SettingsPageState SettingsPageCoordinator::BuildState(
    const wifi_service::UiState& wifi_state,
    const storage_service::Snapshot& storage_snapshot) const
{
    storage_service::StorageStats storage_stats = {};
    const bool allow_live_storage_stats =
        !storage_service::IsWriteBusy() &&
        storage_snapshot.mode != storage_service::Mode::kFormatting;
    const bool has_storage_stats =
        allow_live_storage_stats && storage_service::GetStorageStats(&storage_stats);

    epaper_ui::SettingsPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.title_text = "Settings";
    state.wifi_toggle = {
        .label_text = "WiFi",
        .toggle_state = BuildToggleState(
            wifi_state.wifi_enabled,
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsWifiToggle)),
    };
    state.access_point_toggle = {
        .label_text = "Access Point",
        .toggle_state = BuildToggleState(
            wifi_state.access_point_mode,
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsEnableApToggle)),
    };

    state.storage_status.has_sd_card =
        storage_snapshot.inserted && storage_snapshot.mounted && has_storage_stats;
    if (state.storage_status.has_sd_card) {
        state.storage_status.free_space_text = FormatStorageBytes(storage_stats.free_bytes);
        state.storage_status.used_percent = storage_stats.used_percent;
    }

    std::string_view format_label = "Format SD";
    if (storage_snapshot.mode == storage_service::Mode::kFormatting ||
        (storage_snapshot.operation == storage_service::Operation::kFormatSd &&
         storage_snapshot.phase == storage_service::OperationPhase::kStarted)) {
        format_label = "Formatting SD";
    }
    state.format_sd_button = {
        .label_text = format_label,
        .selected =
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsFormatSdButton),
    };
    state.manual_onboarding_button = {
        .label_text = "Manual",
        .selected = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsManualOnboardingButton),
    };
    return state;
}

epaper_ui::ToggleVisualState SettingsPageCoordinator::BuildToggleState(bool enabled, bool focused)
{
    if (focused) {
        return enabled ? epaper_ui::ToggleVisualState::kFocusOn
                       : epaper_ui::ToggleVisualState::kFocusOff;
    }
    return enabled ? epaper_ui::ToggleVisualState::kOn
                   : epaper_ui::ToggleVisualState::kOff;
}
