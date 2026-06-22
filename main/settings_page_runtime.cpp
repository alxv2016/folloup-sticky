#include "settings_page_runtime.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>

#include "epaper_ui/settings_page.h"
#include "esp_log.h"
#include "feedback_service.h"
#include "storage_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"

namespace settings_page_runtime {
namespace {

constexpr const char* kTag = "SettingsPageRuntime";
constexpr FocusItem kFocusOrder[] = {
    FocusItem::kWifiToggle,
    FocusItem::kAccessPointToggle,
    FocusItem::kFormatSdButton,
    FocusItem::kFooterSettings,
    FocusItem::kFooterHome,
};

std::mutex s_mutex;
FocusItem s_focused_item = FocusItem::kNone;
int32_t s_interaction_generation = 1;

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

FocusItem FocusItemFromTargetIndex(int32_t index)
{
    switch (static_cast<FocusItem>(index)) {
        case FocusItem::kWifiToggle:
        case FocusItem::kAccessPointToggle:
        case FocusItem::kFormatSdButton:
        case FocusItem::kFooterSettings:
        case FocusItem::kFooterHome:
            return static_cast<FocusItem>(index);
        case FocusItem::kNone:
        default:
            return FocusItem::kNone;
    }
}

FocusItem FocusItemFromUiItem(epaper_ui::SettingsPageItemId item)
{
    switch (item) {
        case epaper_ui::SettingsPageItemId::kWifiToggle:
            return FocusItem::kWifiToggle;
        case epaper_ui::SettingsPageItemId::kAccessPointToggle:
            return FocusItem::kAccessPointToggle;
        case epaper_ui::SettingsPageItemId::kFormatSdButton:
            return FocusItem::kFormatSdButton;
        case epaper_ui::SettingsPageItemId::kNone:
        default:
            return FocusItem::kNone;
    }
}

footer_runtime::FooterFocusItem FooterItemForFocusItem(FocusItem item)
{
    switch (item) {
        case FocusItem::kFooterSettings:
            return footer_runtime::FooterFocusItem::kSettings;
        case FocusItem::kFooterHome:
            return footer_runtime::FooterFocusItem::kHome;
        case FocusItem::kWifiToggle:
        case FocusItem::kAccessPointToggle:
        case FocusItem::kFormatSdButton:
        case FocusItem::kNone:
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

FocusItem FocusItemFromFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return FocusItem::kFooterSettings;
        case footer_runtime::FooterFocusItem::kHome:
            return FocusItem::kFooterHome;
        case footer_runtime::FooterFocusItem::kWifi:
        case footer_runtime::FooterFocusItem::kTime:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        case footer_runtime::FooterFocusItem::kNone:
        default:
            return FocusItem::kNone;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    footer_runtime::ProjectionState projection = {};
    projection.focused_item = FooterItemForFocusItem(s_focused_item);
    return projection;
}

int FocusOrderIndex(FocusItem item)
{
    constexpr int kFocusCount = static_cast<int>(sizeof(kFocusOrder) / sizeof(kFocusOrder[0]));
    for (int index = 0; index < kFocusCount; ++index) {
        if (kFocusOrder[index] == item) {
            return index;
        }
    }
    return -1;
}

int WrapFocusIndex(int index)
{
    constexpr int kFocusCount = static_cast<int>(sizeof(kFocusOrder) / sizeof(kFocusOrder[0]));
    if (kFocusCount <= 0) {
        return -1;
    }

    int wrapped = index % kFocusCount;
    if (wrapped < 0) {
        wrapped += kFocusCount;
    }
    return wrapped;
}

esp_err_t SyncFocusUi(display_service::RefreshMode refresh_mode)
{
    const esp_err_t settings_err = UpdateDisplayStateAndRequestRefresh(refresh_mode);
    if (settings_err != ESP_OK && settings_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Settings page refresh after focus update failed: %s",
                 esp_err_to_name(settings_err));
        return settings_err;
    }

    const esp_err_t footer_err = footer_runtime::UpdateDisplayStateAndRequestRefresh(refresh_mode);
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer refresh after settings focus update failed: %s",
                 esp_err_to_name(footer_err));
        return footer_err;
    }

    return ESP_OK;
}

ActivationResult ActivateItem(FocusItem item)
{
    ActivationResult result = {};

    switch (item) {
        case FocusItem::kWifiToggle: {
            const wifi_service::UiState state = wifi_service::GetUiState();
            wifi_service::SetWifiEnabled(!state.wifi_enabled);
            result.handled = true;
            result.play_feedback = true;
            result.feedback_event = feedback_service::FeedbackEvent::kButtonClick;
            break;
        }
        case FocusItem::kAccessPointToggle: {
            const wifi_service::UiState state = wifi_service::GetUiState();
            wifi_service::SetAccessPointEnabled(!state.access_point_mode);
            result.handled = true;
            result.play_feedback = true;
            result.feedback_event = feedback_service::FeedbackEvent::kButtonClick;
            break;
        }
        case FocusItem::kFormatSdButton:
            (void)storage_service::RequestFormatSdCard();
            result.handled = true;
            result.play_feedback = true;
            result.feedback_event = feedback_service::FeedbackEvent::kButtonClick;
            break;
        case FocusItem::kFooterSettings:
            result.handled = true;
            result.play_feedback = true;
            result.feedback_event = feedback_service::FeedbackEvent::kButtonClick;
            result.footer_item = footer_runtime::FooterFocusItem::kSettings;
            break;
        case FocusItem::kFooterHome:
            result.handled = true;
            result.play_feedback = true;
            result.feedback_event = feedback_service::FeedbackEvent::kButtonClick;
            result.footer_item = footer_runtime::FooterFocusItem::kHome;
            break;
        case FocusItem::kNone:
        default:
            break;
    }

    return result;
}

epaper_ui::ToggleVisualState ToggleStateFor(bool enabled, bool focused)
{
    if (focused) {
        return enabled ? epaper_ui::ToggleVisualState::kFocusOn
                       : epaper_ui::ToggleVisualState::kFocusOff;
    }
    return enabled ? epaper_ui::ToggleVisualState::kOn
                   : epaper_ui::ToggleVisualState::kOff;
}

epaper_ui::SettingsPageState BuildStateLocked()
{
    const wifi_service::UiState wifi_state = wifi_service::GetUiState();
    const storage_service::Snapshot storage_snapshot = storage_service::GetSnapshot();

    storage_service::StorageStats storage_stats = {};
    const bool has_storage_stats = storage_service::GetStorageStats(&storage_stats);

    epaper_ui::SettingsPageState state = {};
    state.title_text = "Settings";
    state.wifi_toggle = {
        .label_text = "WiFi",
        .toggle_state = ToggleStateFor(
            wifi_state.wifi_enabled, s_focused_item == FocusItem::kWifiToggle),
    };
    state.access_point_toggle = {
        .label_text = "Access Point",
        .toggle_state = ToggleStateFor(
            wifi_state.access_point_mode, s_focused_item == FocusItem::kAccessPointToggle),
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
        .selected = s_focused_item == FocusItem::kFormatSdButton,
    };
    return state;
}

epaper_ui::SettingsPageState BuildState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildStateLocked();
}

bool ResolveTouchTargetImpl(int x,
                            int y,
                            app_interaction::InteractiveTarget* target,
                            void*)
{
    if (target != nullptr) {
        *target = {};
    }

    const epaper_ui::SettingsPageState state = BuildState();
    epaper_ui::SettingsPageItemId item = epaper_ui::SettingsPageItemId::kNone;
    if (!epaper_ui::HitTestSettingsPageItem(display_service::PortraitWidth(),
                                            display_service::PortraitHeight(),
                                            state,
                                            x,
                                            y,
                                            &item)) {
        return false;
    }

    const FocusItem focus_item = FocusItemFromUiItem(item);
    if (focus_item == FocusItem::kNone) {
        return false;
    }

    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        generation = s_interaction_generation;
    }
    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kPage,
            .kind = app_interaction::Kind::kPageAction,
            .primary_index = static_cast<int32_t>(focus_item),
            .secondary_index = generation,
        };
    }
    return true;
}

bool FocusTouchTargetImpl(const app_interaction::InteractiveTarget& target, void*)
{
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return false;
    }

    const FocusItem item = FocusItemFromTargetIndex(target.primary_index);
    if (item == FocusItem::kNone) {
        return false;
    }

    bool changed = false;
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale settings focus target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return false;
        }
        if (s_focused_item != item) {
            s_focused_item = item;
            projection = BuildFooterProjectionStateLocked();
            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    footer_runtime::SetProjectionState(projection);
    (void)SyncFocusUi(display_service::RefreshMode::kPartial);
    return true;
}

app_interaction::InputResult ActivateTouchTargetImpl(const app_interaction::InteractiveTarget& target,
                                                     void*)
{
    app_interaction::InputResult result = {};
    if (target.owner != app_interaction::Owner::kPage ||
        target.kind != app_interaction::Kind::kPageAction) {
        return result;
    }

    const FocusItem item = FocusItemFromTargetIndex(target.primary_index);
    if (item == FocusItem::kNone) {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale settings activate target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return result;
        }
    }

    const ActivationResult activation = ActivateItem(item);
    result.consumed = activation.handled;
    if (activation.handled) {
        if (activation.play_feedback) {
            (void)feedback_service::Play(activation.feedback_event);
        }
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return result;
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    return display_service::SetSettingsPageState(BuildState());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kSettingsPage, &UpdateDisplayState, refresh_mode);
}

bool MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }

    bool changed = false;
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        int current_index = FocusOrderIndex(s_focused_item);
        if (current_index < 0) {
            current_index = 0;
        }

        const int next_index = WrapFocusIndex(current_index + (delta > 0 ? 1 : -1));
        if (next_index >= 0 && kFocusOrder[next_index] != s_focused_item) {
            s_focused_item = kFocusOrder[next_index];
            projection = BuildFooterProjectionStateLocked();
            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    footer_runtime::SetProjectionState(projection);
    (void)SyncFocusUi(display_service::RefreshMode::kPartial);
    return true;
}

ActivationResult ActivateFocusedItem()
{
    FocusItem focused_item = FocusItem::kNone;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        focused_item = s_focused_item;
    }

    const ActivationResult result = ActivateItem(focused_item);
    if (result.handled && result.footer_item == footer_runtime::FooterFocusItem::kNone) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return result;
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

bool SyncFocusFromFooterProjection()
{
    const FocusItem footer_focus =
        FocusItemFromFooterItem(footer_runtime::GetProjectionState().focused_item);
    if (footer_focus == FocusItem::kNone) {
        return false;
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_focused_item != footer_focus) {
            s_focused_item = footer_focus;
            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    const esp_err_t err = UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Settings page refresh after footer focus sync failed: %s",
                 esp_err_to_name(err));
    }
    return true;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_focused_item = FocusItem::kWifiToggle;
        if (s_interaction_generation == INT32_MAX) {
            s_interaction_generation = 1;
        } else {
            ++s_interaction_generation;
        }
        projection = BuildFooterProjectionStateLocked();
    }

    footer_runtime::SetProjectionState(projection);
}

page_interaction_runtime::TouchProvider BuildTouchProvider()
{
    return {
        .resolve_touch_target = &ResolveTouchTargetImpl,
        .focus_touch_target = &FocusTouchTargetImpl,
        .activate_touch_target = &ActivateTouchTargetImpl,
        .context = nullptr,
    };
}

}  // namespace settings_page_runtime
