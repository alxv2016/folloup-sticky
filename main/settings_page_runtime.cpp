#include "settings_page_runtime.h"

#include <climits>
#include <mutex>

#include "esp_log.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "settings_page_coordinator.h"
#include "storage_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"

namespace settings_page_runtime {
namespace {

constexpr const char* kTag = "SettingsPageRuntime";

std::mutex s_mutex;
SettingsPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
ActionHandler s_action_handler = nullptr;
void* s_action_context = nullptr;
RefreshHandler s_refresh_handler = nullptr;
void* s_refresh_context = nullptr;

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
    } else {
        ++s_interaction_generation;
    }
}

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kTime:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

page_navigation::NavigationItemRole RoleForUiItem(epaper_ui::SettingsPageItemId item)
{
    switch (item) {
        case epaper_ui::SettingsPageItemId::kWifiToggle:
            return page_navigation::NavigationItemRole::kSettingsWifiToggle;
        case epaper_ui::SettingsPageItemId::kAccessPointToggle:
            return page_navigation::NavigationItemRole::kSettingsEnableApToggle;
        case epaper_ui::SettingsPageItemId::kFormatSdButton:
            return page_navigation::NavigationItemRole::kSettingsFormatSdButton;
        case epaper_ui::SettingsPageItemId::kNone:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kSettingsPageMenu,
                                          s_coordinator.focus().index(),
                                          -1,
                                          -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

epaper_ui::SettingsPageState BuildStateLocked()
{
    return s_coordinator.BuildState(wifi_service::GetUiState(), storage_service::GetSnapshot());
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

ActivationResult ActivateFocusedRoleLocked()
{
    ActivationResult result = {};
    ActionHandler action_handler = s_action_handler;
    void* action_context = s_action_context;

    const page_navigation::NavigationItemDescriptor* item =
        s_coordinator.navigation_model().ItemAt(s_coordinator.focus().index());
    const page_navigation::NavigationItemRole role =
        item != nullptr ? item->role : page_navigation::NavigationItemRole::kUnknown;

    switch (role) {
        case page_navigation::NavigationItemRole::kSettingsWifiToggle: {
            const wifi_service::UiState state = wifi_service::GetUiState();
            wifi_service::SetWifiEnabled(!state.wifi_enabled);
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        }
        case page_navigation::NavigationItemRole::kSettingsEnableApToggle: {
            const wifi_service::UiState state = wifi_service::GetUiState();
            wifi_service::SetAccessPointEnabled(!state.access_point_mode);
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        }
        case page_navigation::NavigationItemRole::kSettingsFormatSdButton:
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        case page_navigation::NavigationItemRole::kFooterSettings:
            result.handled = true;
            result.footer_item = footer_runtime::FooterFocusItem::kSettings;
            break;
        case page_navigation::NavigationItemRole::kFooterWifi:
            result.handled = true;
            result.footer_item = footer_runtime::FooterFocusItem::kWifi;
            break;
        case page_navigation::NavigationItemRole::kFooterHome:
            result.handled = true;
            result.footer_item = footer_runtime::FooterFocusItem::kHome;
            break;
        case page_navigation::NavigationItemRole::kUnknown:
        default:
            break;
    }

    if (role == page_navigation::NavigationItemRole::kSettingsFormatSdButton &&
        action_handler != nullptr) {
        action_handler(ActionRequest::kShowFormatSdModal, action_context);
    }

    return result;
}

bool ResolveTouchTargetImpl(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    if (target != nullptr) {
        *target = {};
    }

    const epaper_ui::SettingsPageState state = [&]() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return BuildStateLocked();
    }();

    epaper_ui::SettingsPageItemId item = epaper_ui::SettingsPageItemId::kNone;
    if (!epaper_ui::HitTestSettingsPageItem(display_service::PortraitWidth(),
                                            display_service::PortraitHeight(),
                                            state,
                                            x,
                                            y,
                                            &item)) {
        return false;
    }

    const page_navigation::NavigationItemRole role = RoleForUiItem(item);
    const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
    if (role == page_navigation::NavigationItemRole::kUnknown || focus_index < 0) {
        return false;
    }

    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kPage,
            .kind = app_interaction::Kind::kPageAction,
            .primary_index = focus_index,
            .secondary_index = s_interaction_generation,
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
        changed = s_coordinator.SetFocusIndex(target.primary_index);
        if (changed) {
            projection = BuildFooterProjectionStateLocked();
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

    ActivationResult activation = {};
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
        (void)s_coordinator.SetFocusIndex(target.primary_index);
        activation = ActivateFocusedRoleLocked();
    }

    result.consumed = activation.handled;
    result.play_feedback = activation.play_feedback;
    result.feedback_cue = activation.feedback_cue;
    if (activation.handled) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return result;
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetSettingsPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    RefreshHandler refresh_handler = nullptr;
    void* refresh_context = nullptr;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        refresh_handler = s_refresh_handler;
        refresh_context = s_refresh_context;
    }
    if (refresh_handler != nullptr) {
        return refresh_handler(refresh_mode, refresh_context);
    }

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
        changed = s_coordinator.MoveFocus(delta);
        if (changed) {
            projection = BuildFooterProjectionStateLocked();
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
    ActivationResult result = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        result = ActivateFocusedRoleLocked();
    }

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
    const page_navigation::NavigationItemRole role =
        FooterRoleForFooterItem(footer_runtime::GetProjectionState().focused_item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return false;
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index >= 0) {
            changed = s_coordinator.SetFocusIndex(focus_index);
        }
    }

    if (!changed) {
        return false;
    }

    const esp_err_t err =
        UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
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
        s_coordinator.Show();
        AdvanceInteractionGenerationLocked();
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

void SetActionHandler(ActionHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_action_handler = handler;
    s_action_context = context;
}

void SetRefreshHandler(RefreshHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_refresh_handler = handler;
    s_refresh_context = context;
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
