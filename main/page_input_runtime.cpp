#include "page_input_runtime.h"

#include "overlay_runtime.h"
#include "page_interaction_runtime.h"
#include "settings_page_interactions.h"
#include "settings_page_runtime.h"
#include "storage_service.h"
#include "time_page_interactions.h"
#include "time_page_runtime.h"
#include "ui_refresh_runtime.h"
#include "wifi_page_interactions.h"
#include "wifi_page_runtime.h"
#include "wifi_service.h"

namespace page_input_runtime {
namespace {

footer_runtime::FooterFocusItem FooterItemFromTargetIndex(int32_t index)
{
    switch (static_cast<footer_runtime::FooterFocusItem>(index)) {
        case footer_runtime::FooterFocusItem::kHome:
        case footer_runtime::FooterFocusItem::kSettings:
        case footer_runtime::FooterFocusItem::kWifi:
            return static_cast<footer_runtime::FooterFocusItem>(index);
        case footer_runtime::FooterFocusItem::kTime:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        case footer_runtime::FooterFocusItem::kNone:
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

app_interaction::InputResult MakeConsumedResult(bool play_click_feedback)
{
    app_interaction::InputResult result = {};
    result.consumed = true;
    if (play_click_feedback) {
        result.play_feedback = true;
        result.feedback_cue = app_interaction::FeedbackCue::kClick;
    }
    return result;
}

void ApplySettingsPageStateUpdate(display_service::RefreshMode refresh_mode)
{
    (void)settings_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_mode);
}

void ApplySettingsPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)settings_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

esp_err_t ApplySettingsPageAndFooterDisplayState()
{
    const esp_err_t page_err = settings_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }

    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }

    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyWifiPageStateUpdate(display_service::RefreshMode refresh_mode)
{
    (void)wifi_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_mode);
}

void ApplyWifiPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)wifi_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

esp_err_t ApplyWifiPageAndFooterDisplayState()
{
    const esp_err_t page_err = wifi_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }

    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }

    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyTimePageStateUpdate(display_service::RefreshMode refresh_mode)
{
    (void)time_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_mode);
}

void ApplyTimePageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)time_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

esp_err_t ApplyTimePageAndFooterDisplayState()
{
    const esp_err_t page_err = time_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }

    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }

    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplySettingsFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }

    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(settings_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kSettingsPage,
                                               &ApplySettingsPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }

        ApplySettingsPageStateUpdate(refresh_request);
    }
}

void ApplyWifiFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }

    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(wifi_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kWifiPage,
                                               &ApplyWifiPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }

        ApplyWifiPageStateUpdate(refresh_request);
    }
}

void ApplyTimeFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }

    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(time_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTimePage,
                                               &ApplyTimePageAndFooterDisplayState,
                                               refresh_request);
            return;
        }

        ApplyTimePageStateUpdate(refresh_request);
    }
}

ButtonResult ApplySettingsActivateResult(const settings_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    settings_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.force_refresh = []() {
        ApplySettingsPageStateUpdate(display_service::RefreshMode::kFull);
    };
    callbacks.toggle_wifi = []() {
        const wifi_service::UiState state = wifi_service::GetUiState();
        wifi_service::SetWifiEnabled(!state.wifi_enabled);
    };
    callbacks.toggle_access_point = []() {
        const wifi_service::UiState state = wifi_service::GetUiState();
        wifi_service::SetAccessPointEnabled(!state.access_point_mode);
    };
    callbacks.show_format_sd_modal = []() {
        const storage_service::Snapshot snapshot = storage_service::GetSnapshot();
        if (!snapshot.inserted) {
            (void)overlay_runtime::ShowStorageModalNoSdCard();
            return;
        }
        (void)overlay_runtime::ShowStorageModalConfirmFormat();
    };
    settings_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

ButtonResult ApplyWifiActivateResult(const wifi_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    wifi_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.force_refresh = []() {
        ApplyWifiPageStateUpdate(display_service::RefreshMode::kFull);
    };
    callbacks.open_password_keyboard = []() {
        (void)wifi_page_runtime::ShowPasswordKeyboard();
    };
    callbacks.start_network_scan = []() {
        (void)wifi_service::StartNetworkScan();
    };
    callbacks.toggle_selected_network_connection =
        [](bool disconnect_current_network,
           const std::string& ssid,
           const std::string& password) {
            if (disconnect_current_network) {
                (void)wifi_service::DisconnectFromNetwork(false);
                return;
            }
            if (ssid.empty()) {
                return;
            }
            (void)wifi_service::ConnectToNetwork(ssid, password, true);
    };
    wifi_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }

    if (activation.apply_page_state) {
        ApplyWifiPageStateUpdate(display_service::RefreshMode::kPartial);
    }
    return result;
}

ButtonResult ApplyWifiSecondaryActivateResult(
    const wifi_page_interactions::SecondaryActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result.consumed = true;

    wifi_page_interactions::SecondaryActivateCallbacks callbacks = {};
    callbacks.play_activate_cue = [&result]() {
        result.interaction_result.play_feedback = true;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kClick;
    };
    callbacks.apply_page_state = []() {
        ApplyWifiPageStateUpdate(display_service::RefreshMode::kPartial);
    };
    wifi_page_interactions::ApplySecondaryActivateResult(activation, callbacks);
    return result;
}

ButtonResult ApplyTimeActivateResult(const time_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    time_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.show_timezone_modal = []() {
        (void)time_page_runtime::ShowTimezoneModal();
    };
    callbacks.edit_hour = []() {
        (void)time_page_runtime::ShowFieldKeyboard(
            page_navigation::NavigationItemRole::kTimePageHour);
    };
    callbacks.edit_minute = []() {
        (void)time_page_runtime::ShowFieldKeyboard(
            page_navigation::NavigationItemRole::kTimePageMinute);
    };
    callbacks.edit_month = []() {
        (void)time_page_runtime::ShowFieldKeyboard(
            page_navigation::NavigationItemRole::kTimePageMonth);
    };
    callbacks.edit_day = []() {
        (void)time_page_runtime::ShowFieldKeyboard(
            page_navigation::NavigationItemRole::kTimePageDay);
    };
    callbacks.edit_year = []() {
        (void)time_page_runtime::ShowFieldKeyboard(
            page_navigation::NavigationItemRole::kTimePageYear);
    };
    callbacks.toggle_meridiem = []() {
        time_page_runtime::ToggleMeridiem();
    };
    callbacks.save = []() {
        (void)time_page_runtime::Save();
    };
    time_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }

    if (activation.apply_page_state) {
        ApplyTimePageStateUpdate(display_service::RefreshMode::kPartial);
    }
    return result;
}

FocusMoveResult ApplySettingsMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplySettingsFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

FocusMoveResult ApplyWifiMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyWifiFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

FocusMoveResult ApplyTimeMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyTimeFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

bool ResolveSettingsTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return settings_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusSettingsTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome =
        settings_page_runtime::FocusTouchTarget(target);
    ApplySettingsFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateSettingsTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplySettingsActivateResult(settings_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

bool ResolveWifiTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return wifi_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusSettingsFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        settings_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplySettingsFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusWifiTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome = wifi_page_runtime::FocusTouchTarget(target);
    ApplyWifiFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusWifiFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        wifi_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplyWifiFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateWifiTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplyWifiActivateResult(wifi_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

bool ResolveTimeTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return time_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusTimeTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome = time_page_runtime::FocusTouchTarget(target);
    ApplyTimeFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusTimeFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        time_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplyTimeFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateTimeTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplyTimeActivateResult(time_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

page_interaction_runtime::TouchProvider TouchProviderForScreen(display_service::ScreenId screen)
{
    switch (screen) {
        case display_service::ScreenId::kSettings:
            return {
                .resolve_touch_target = &ResolveSettingsTouchTarget,
                .focus_touch_target = &FocusSettingsTouchTarget,
                .activate_touch_target = &ActivateSettingsTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kWifi:
            return {
                .resolve_touch_target = &ResolveWifiTouchTarget,
                .focus_touch_target = &FocusWifiTouchTarget,
                .activate_touch_target = &ActivateWifiTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kTime:
            return {
                .resolve_touch_target = &ResolveTimeTouchTarget,
                .focus_touch_target = &FocusTimeTouchTarget,
                .activate_touch_target = &ActivateTimeTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

ButtonResult HandleSettingsButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};
    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplySettingsActivateResult(settings_page_runtime::ActivateFocusedItem());
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kPressUp:
        case button_service::ButtonEvent::kPressRepeat:
        case button_service::ButtonEvent::kLongPressStart:
        case button_service::ButtonEvent::kLongPressUp:
            result.handled = true;
            result.interaction_result.consumed = true;
            return result;
        case button_service::ButtonEvent::kDoubleClick:
        default:
            return result;
    }
}

ButtonResult HandleWifiButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            if (event.button != button_service::ButtonId::kPowerOk) {
                return result;
            }
            return ApplyWifiActivateResult(wifi_page_runtime::ActivateFocusedItem());
        case button_service::ButtonEvent::kDoubleClick:
            if (event.button != button_service::ButtonId::kUp) {
                return result;
            }
            return ApplyWifiSecondaryActivateResult(
                wifi_page_runtime::SecondaryActivateFocusedItem());
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kPressUp:
        case button_service::ButtonEvent::kPressRepeat:
        case button_service::ButtonEvent::kLongPressStart:
        case button_service::ButtonEvent::kLongPressUp:
            if (event.button != button_service::ButtonId::kPowerOk) {
                return result;
            }
            result.handled = true;
            result.interaction_result.consumed = true;
            return result;
        default:
            return result;
    }
}

ButtonResult HandleTimeButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};
    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyTimeActivateResult(time_page_runtime::ActivateFocusedItem());
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kPressUp:
        case button_service::ButtonEvent::kPressRepeat:
        case button_service::ButtonEvent::kLongPressStart:
        case button_service::ButtonEvent::kLongPressUp:
            result.handled = true;
            result.interaction_result.consumed = true;
            return result;
        case button_service::ButtonEvent::kDoubleClick:
        default:
            return result;
    }
}

}  // namespace

void ConfigureTouchProviderForScreen(display_service::ScreenId screen)
{
    const page_interaction_runtime::TouchProvider provider = TouchProviderForScreen(screen);
    if (provider.resolve_touch_target == nullptr) {
        page_interaction_runtime::ClearTouchProvider();
        return;
    }

    page_interaction_runtime::RegisterTouchProvider(provider);
}

footer_runtime::ProjectionState BuildFooterProjectionForScreen(display_service::ScreenId screen)
{
    switch (screen) {
        case display_service::ScreenId::kSettings:
            return settings_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kWifi:
            return wifi_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kTime:
            return time_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

void ResetFocusForScreen(display_service::ScreenId screen)
{
    switch (screen) {
        case display_service::ScreenId::kSettings:
            settings_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kWifi:
            wifi_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kTime:
            time_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return;
    }
}

bool FocusFooterTouchTargetForCurrentScreen(const app_interaction::InteractiveTarget& target)
{
    if (target.owner != app_interaction::Owner::kFooter ||
        target.kind != app_interaction::Kind::kFooterItem) {
        return false;
    }

    switch (display_service::GetCurrentScreen()) {
        case display_service::ScreenId::kSettings:
            return FocusSettingsFooterTarget(target);
        case display_service::ScreenId::kWifi:
            return FocusWifiFooterTarget(target);
        case display_service::ScreenId::kTime:
            return FocusTimeFooterTarget(target);
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return false;
    }
}

FocusMoveResult MoveFocusForCurrentScreen(int delta, bool page_jump)
{
    switch (display_service::GetCurrentScreen()) {
        case display_service::ScreenId::kSettings:
            return ApplySettingsMoveResult(settings_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kWifi:
            return ApplyWifiMoveResult(wifi_page_runtime::MoveFocus(delta, page_jump));
        case display_service::ScreenId::kTime:
            return ApplyTimeMoveResult(time_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

ButtonResult HandleButtonEventForCurrentScreen(const button_service::ButtonEventInfo& event)
{
    switch (display_service::GetCurrentScreen()) {
        case display_service::ScreenId::kSettings:
            return HandleSettingsButtonEvent(event);
        case display_service::ScreenId::kWifi:
            return HandleWifiButtonEvent(event);
        case display_service::ScreenId::kTime:
            return HandleTimeButtonEvent(event);
        case display_service::ScreenId::kHome:
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

}  // namespace page_input_runtime
