#include "wifi_page_runtime.h"

#include <climits>
#include <mutex>

#include "epaper_ui/keyboard_controller.h"
#include "epaper_ui/password_input.h"
#include "esp_log.h"
#include "overlay_runtime.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "ui_refresh_runtime.h"
#include "wifi_page_coordinator.h"
#include "wifi_service.h"

namespace wifi_page_runtime {
namespace {

constexpr const char* kTag = "WifiPageRuntime";

std::mutex s_mutex;
WifiPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
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

page_navigation::NavigationItemRole RoleForUiItem(epaper_ui::WifiPageItemId item)
{
    switch (item) {
        case epaper_ui::WifiPageItemId::kNetworkList:
            return page_navigation::NavigationItemRole::kWifiPageNetworkList;
        case epaper_ui::WifiPageItemId::kPasswordInput:
            return page_navigation::NavigationItemRole::kWifiPagePasswordInput;
        case epaper_ui::WifiPageItemId::kPasswordVisibilityButton:
            return page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton;
        case epaper_ui::WifiPageItemId::kScanButton:
            return page_navigation::NavigationItemRole::kWifiPageScanButton;
        case epaper_ui::WifiPageItemId::kConnectButton:
            return page_navigation::NavigationItemRole::kWifiPageConnectButton;
        case epaper_ui::WifiPageItemId::kNone:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kWifiPageControls,
                                          s_coordinator.focus().index(),
                                          -1,
                                          -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

epaper_ui::WifiPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

esp_err_t SyncFocusUi(display_service::RefreshMode refresh_mode, bool include_footer)
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

    const esp_err_t page_err = UpdateDisplayStateAndRequestRefresh(refresh_mode);
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    if (!include_footer) {
        return ESP_OK;
    }
    return footer_runtime::UpdateDisplayStateAndRequestRefresh(refresh_mode);
}

void KeyboardStateChanged(const epaper_ui::KeyboardState& keyboard_state,
                          epaper_ui::KeyboardIntent intent,
                          void*)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        epaper_ui::ApplyKeyboardInputToPasswordInput(&s_coordinator.password_input_state(),
                                                     keyboard_state.input);
        s_coordinator.password_input_state().focused = true;
        s_coordinator.password_input_state().active =
            intent == epaper_ui::KeyboardIntent::kNone;
        if (intent != epaper_ui::KeyboardIntent::kNone) {
            const int focus_index = s_coordinator.navigation_model().IndexOfRole(
                page_navigation::NavigationItemRole::kWifiPagePasswordInput);
            if (focus_index >= 0) {
                (void)s_coordinator.SetFocusIndex(focus_index);
            }
        }
    }

    if (intent == epaper_ui::KeyboardIntent::kSubmit ||
        intent == epaper_ui::KeyboardIntent::kDismiss) {
        (void)overlay_runtime::DismissKeyboard();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
}

esp_err_t ShowPasswordKeyboard()
{
    epaper_ui::KeyboardState keyboard_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        keyboard_state.visible = true;
        keyboard_state.title_text = "WiFi Password";
        keyboard_state.input =
            epaper_ui::PasswordInputToKeyboardInput(s_coordinator.password_input_state());
        keyboard_state.input.focused = true;
        keyboard_state.input.active = true;
        keyboard_state.layout = epaper_ui::KeyboardLayoutKind::kLettersLower;
        keyboard_state.selected_key_index = 0;
        keyboard_state.shift_locked = false;
        s_coordinator.password_input_state().focused = true;
        s_coordinator.password_input_state().active = true;
    }
    return overlay_runtime::ShowKeyboard(keyboard_state, &KeyboardStateChanged, nullptr);
}

ActivationResult ActivateFocusedRoleLocked()
{
    ActivationResult result = {};
    const page_navigation::NavigationItemDescriptor* item =
        s_coordinator.navigation_model().ItemAt(s_coordinator.focus().index());
    const page_navigation::NavigationItemRole role =
        item != nullptr ? item->role : page_navigation::NavigationItemRole::kUnknown;

    switch (role) {
        case page_navigation::NavigationItemRole::kWifiPageNetworkList:
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            if (s_coordinator.network_list_active()) {
                (void)s_coordinator.CommitFocusedNetworkSelection();
                (void)s_coordinator.ExitNetworkList();
            } else if (s_coordinator.HasNetworks()) {
                (void)s_coordinator.EnterNetworkList();
            }
            break;
        case page_navigation::NavigationItemRole::kWifiPagePasswordInput:
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        case page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton:
            (void)s_coordinator.TogglePasswordVisibility();
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        case page_navigation::NavigationItemRole::kWifiPageScanButton:
            (void)wifi_service::StartNetworkScan();
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        case page_navigation::NavigationItemRole::kWifiPageConnectButton: {
            const std::string ssid = s_coordinator.SelectedNetworkSsid();
            if (!ssid.empty()) {
                if (s_coordinator.SelectedNetworkIsCurrent()) {
                    (void)wifi_service::DisconnectFromNetwork(false);
                } else {
                    (void)wifi_service::ConnectToNetwork(
                        ssid,
                        s_coordinator.password_input_state().value_text,
                        true);
                }
            }
            result.handled = true;
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            break;
        }
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

    return result;
}

bool ResolveTouchTargetImpl(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    if (target != nullptr) {
        *target = {};
    }

    const epaper_ui::WifiPageState state = [&]() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return BuildStateLocked();
    }();

    int row_index = epaper_ui::kNetworkListNoSelection;
    if (epaper_ui::HitTestWifiPageNetworkRow(display_service::PortraitWidth(),
                                             display_service::PortraitHeight(),
                                             state,
                                             x,
                                             y,
                                             &row_index)) {
        if (target != nullptr) {
            *target = {
                .owner = app_interaction::Owner::kPage,
                .kind = app_interaction::Kind::kPageListRow,
                .primary_index = row_index,
                .secondary_index = s_interaction_generation,
            };
        }
        return true;
    }

    epaper_ui::WifiPageItemId item = epaper_ui::WifiPageItemId::kNone;
    if (!epaper_ui::HitTestWifiPageItem(display_service::PortraitWidth(),
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
    if (target.owner != app_interaction::Owner::kPage) {
        return false;
    }

    bool changed = false;
    bool footer_changed = false;
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            return false;
        }

        const footer_runtime::ProjectionState previous_projection =
            BuildFooterProjectionStateLocked();
        if (target.kind == app_interaction::Kind::kPageListRow) {
            changed = s_coordinator.FocusNetworkRow(target.primary_index);
        } else if (target.kind == app_interaction::Kind::kPageAction) {
            changed = s_coordinator.SetFocusIndex(target.primary_index);
            const page_navigation::NavigationItemDescriptor* item =
                s_coordinator.navigation_model().ItemAt(target.primary_index);
            if (item != nullptr &&
                item->role != page_navigation::NavigationItemRole::kWifiPageNetworkList) {
                (void)s_coordinator.ExitNetworkList();
            }
        } else {
            return false;
        }

        if (changed) {
            projection = BuildFooterProjectionStateLocked();
            footer_changed = projection.focused_item != previous_projection.focused_item;
        }
    }

    if (!changed) {
        return false;
    }

    if (footer_changed) {
        footer_runtime::SetProjectionState(projection);
    }
    (void)SyncFocusUi(display_service::RefreshMode::kPartial, footer_changed);
    return true;
}

app_interaction::InputResult ActivateTouchTargetImpl(const app_interaction::InteractiveTarget& target,
                                                     void*)
{
    app_interaction::InputResult result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }

    bool show_keyboard = false;
    ActivationResult activation = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (target.secondary_index != s_interaction_generation) {
            return result;
        }

        if (target.kind == app_interaction::Kind::kPageListRow) {
            if (!s_coordinator.FocusNetworkRow(target.primary_index) ||
                !s_coordinator.SelectNetworkRow(target.primary_index)) {
                return result;
            }
            activation = {
                .handled = true,
                .play_feedback = true,
                .feedback_cue = app_interaction::FeedbackCue::kClick,
            };
        } else if (target.kind == app_interaction::Kind::kPageAction) {
            const page_navigation::NavigationItemDescriptor* item =
                s_coordinator.navigation_model().ItemAt(target.primary_index);
            if (item == nullptr) {
                return result;
            }
            (void)s_coordinator.SetFocusIndex(target.primary_index);
            if (item->role == page_navigation::NavigationItemRole::kWifiPagePasswordInput) {
                show_keyboard = true;
                activation = {
                    .handled = true,
                    .play_feedback = true,
                    .feedback_cue = app_interaction::FeedbackCue::kClick,
                };
            } else {
                activation = ActivateFocusedRoleLocked();
            }
        } else {
            return result;
        }
    }

    result.consumed = activation.handled;
    result.play_feedback = activation.play_feedback;
    result.feedback_cue = activation.feedback_cue;
    if (show_keyboard) {
        (void)ShowPasswordKeyboard();
    }
    (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    return result;
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetWifiPageState(BuildStateLocked());
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
        ui_refresh_runtime::SurfaceKey::kWifiPage, &UpdateDisplayState, refresh_mode);
}

bool MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }

    bool changed = false;
    bool footer_changed = false;
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const footer_runtime::ProjectionState previous_projection =
            BuildFooterProjectionStateLocked();
        changed = s_coordinator.MoveFocus(delta);
        if (changed) {
            projection = BuildFooterProjectionStateLocked();
            footer_changed = projection.focused_item != previous_projection.focused_item;
        }
    }

    if (!changed) {
        return false;
    }

    if (footer_changed) {
        footer_runtime::SetProjectionState(projection);
    }
    (void)SyncFocusUi(display_service::RefreshMode::kPartial, footer_changed);
    return true;
}

bool EnterNetworkList()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        changed = s_coordinator.EnterNetworkList();
    }
    if (changed) {
        (void)SyncFocusUi(display_service::RefreshMode::kPartial, false);
    }
    return changed;
}

bool CommitNetworkListSelectionAndExit()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_coordinator.network_list_active()) {
            return false;
        }
        (void)s_coordinator.CommitFocusedNetworkSelection();
        changed = s_coordinator.ExitNetworkList();
    }
    if (changed) {
        (void)SyncFocusUi(display_service::RefreshMode::kPartial, false);
    }
    return changed;
}

bool ExitNetworkListWithoutSelection()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        changed = s_coordinator.ExitNetworkList();
    }
    if (changed) {
        (void)SyncFocusUi(display_service::RefreshMode::kPartial, false);
    }
    return changed;
}

ActivationResult ActivateFocusedItem()
{
    ActivationResult result = {};
    bool show_keyboard = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const page_navigation::NavigationItemDescriptor* item =
            s_coordinator.navigation_model().ItemAt(s_coordinator.focus().index());
        if (item != nullptr &&
            item->role == page_navigation::NavigationItemRole::kWifiPagePasswordInput) {
            show_keyboard = true;
            result = {
                .handled = true,
                .play_feedback = true,
                .feedback_cue = app_interaction::FeedbackCue::kClick,
            };
        } else {
            result = ActivateFocusedRoleLocked();
        }
    }

    if (show_keyboard) {
        (void)ShowPasswordKeyboard();
    }
    if (result.handled && result.footer_item == footer_runtime::FooterFocusItem::kNone) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return result;
}

bool SecondaryActivateFocusedItem()
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const page_navigation::NavigationItemDescriptor* item =
            s_coordinator.navigation_model().ItemAt(s_coordinator.focus().index());
        if (item == nullptr) {
            return false;
        }
        if (item->role == page_navigation::NavigationItemRole::kWifiPagePasswordVisibilityButton) {
            (void)s_coordinator.TogglePasswordVisibility();
            changed = true;
        }
    }

    if (changed) {
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return changed;
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
        ESP_LOGW(kTag, "WiFi page refresh after footer focus sync failed: %s",
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

void SetRefreshHandler(RefreshHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_refresh_handler = handler;
    s_refresh_context = context;
}

esp_err_t SyncFromService(bool request_refresh_if_active)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RefreshFromService(wifi_service::GetUiState(), wifi_service::GetScanSnapshot());
    }

    const esp_err_t err =
        request_refresh_if_active
            ? UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial)
            : UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "WiFi page sync failed: %s", esp_err_to_name(err));
    }
    return err;
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

}  // namespace wifi_page_runtime
