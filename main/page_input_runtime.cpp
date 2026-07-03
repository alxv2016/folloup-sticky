#include "page_input_runtime.h"

#include "dashboard_page_interactions.h"
#include "dashboard_page_runtime.h"
#include "notes_page_interactions.h"
#include "notes_page_runtime.h"
#include "overlay_runtime.h"
#include "page_interaction_runtime.h"
#include "settings_page_interactions.h"
#include "settings_page_runtime.h"
#include "storage_service.h"
#include "summarize_page_interactions.h"
#include "summarize_page_runtime.h"
#include "time_page_interactions.h"
#include "time_page_runtime.h"
#include "ui_refresh_runtime.h"
#include "vibe_check_page_interactions.h"
#include "vibe_check_page_runtime.h"
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
        case footer_runtime::FooterFocusItem::kTime:
            return static_cast<footer_runtime::FooterFocusItem>(index);
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

void ApplyDashboardPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)dashboard_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

esp_err_t ApplyDashboardPageAndFooterDisplayState()
{
    const esp_err_t page_err = dashboard_page_runtime::UpdateDisplayState();
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

void ApplyDashboardFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }

    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(dashboard_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kDashboardPage,
                                               &ApplyDashboardPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }

        ApplyDashboardPageStateUpdate(refresh_request);
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
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
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
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
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
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
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

ButtonResult ApplyDashboardActivateResult(
    const dashboard_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    dashboard_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.open_menu_item = [](int menu_index) {
        dashboard_page_runtime::OpenMenuItem(menu_index);
    };
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
    };
    dashboard_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
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

FocusMoveResult ApplyDashboardMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyDashboardFocusUpdate({
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

bool ResolveDashboardTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return dashboard_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusDashboardTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome =
        dashboard_page_runtime::FocusTouchTarget(target);
    ApplyDashboardFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusDashboardFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        dashboard_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplyDashboardFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateDashboardTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplyDashboardActivateResult(dashboard_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

esp_err_t ApplyVibeCheckPageAndFooterDisplayState()
{
    const esp_err_t page_err = vibe_check_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyVibeCheckPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)vibe_check_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplyVibeCheckFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(vibe_check_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kVibeCheckPage,
                                               &ApplyVibeCheckPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplyVibeCheckPageStateUpdate(refresh_request);
    }
}

ButtonResult ApplyVibeCheckActivateResult(
    const vibe_check_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    vibe_check_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
    };
    callbacks.enter_card = []() { vibe_check_page_runtime::EnterFocusedCard(); };
    callbacks.refresh_idea = []() { vibe_check_page_runtime::RefreshIdea(); };
    callbacks.delete_idea = []() { vibe_check_page_runtime::DeleteCurrentIdea(); };
    callbacks.pin_idea = []() { vibe_check_page_runtime::PinCurrentIdea(); };
    callbacks.transcribe_idea = []() { vibe_check_page_runtime::TranscribeCurrentIdea(); };
    vibe_check_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplyVibeCheckMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyVibeCheckFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

bool ResolveVibeCheckTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return vibe_check_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusVibeCheckTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome =
        vibe_check_page_runtime::FocusTouchTarget(target);
    ApplyVibeCheckFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusVibeCheckFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        vibe_check_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplyVibeCheckFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateVibeCheckTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplyVibeCheckActivateResult(vibe_check_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

ButtonResult HandleVibeCheckButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // No dedicated back key: a DOWN double-click leaves the card's action row.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kDoubleClick) {
        if (vibe_check_page_runtime::ExitFocusedCard()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyVibeCheckActivateResult(vibe_check_page_runtime::ActivateFocusedItem());
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

esp_err_t ApplySummarizePageAndFooterDisplayState()
{
    const esp_err_t page_err = summarize_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplySummarizePageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)summarize_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplySummarizeFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(summarize_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kSummarizePage,
                                               &ApplySummarizePageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplySummarizePageStateUpdate(refresh_request);
    }
}

ButtonResult ApplySummarizeActivateResult(
    const summarize_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }

    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    summarize_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
    };
    callbacks.toggle_segment = []() { summarize_page_runtime::ToggleSegment(); };
    callbacks.enter_scroll = []() { summarize_page_runtime::EnterScroll(); };
    callbacks.request_notes_summary = []() { summarize_page_runtime::RequestNotesSummary(); };
    callbacks.request_todos_summary = []() { summarize_page_runtime::RequestTodosSummary(); };
    summarize_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplySummarizeMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplySummarizeFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

bool ResolveSummarizeTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return summarize_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusSummarizeTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome =
        summarize_page_runtime::FocusTouchTarget(target);
    ApplySummarizeFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusSummarizeFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        summarize_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplySummarizeFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateSummarizeTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplySummarizeActivateResult(summarize_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

ButtonResult HandleSummarizeButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: DOWN double-click exits an entered control (segment / scroll).
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kDoubleClick) {
        if (summarize_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplySummarizeActivateResult(summarize_page_runtime::ActivateFocusedItem());
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

// --- Notes page ------------------------------------------------------------

esp_err_t ApplyNotesPageAndFooterDisplayState()
{
    const esp_err_t page_err = notes_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyNotesPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)notes_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplyNotesFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(notes_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kNotesPage,
                                               &ApplyNotesPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplyNotesPageStateUpdate(refresh_request);
    }
}

ButtonResult ApplyNotesActivateResult(const notes_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    // Entering a group mutates page state inside HandlePrimaryActivate; repaint here.
    if (activation.apply_page_state) {
        ApplyNotesPageStateUpdate({
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        });
    }

    notes_page_interactions::ActivateCallbacks callbacks = {};
    callbacks.show_home = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kHome;
    };
    callbacks.show_settings = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kSettings;
    };
    callbacks.show_wifi = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kWifi;
    };
    callbacks.show_time = [&result]() {
        result.footer_item = footer_runtime::FooterFocusItem::kTime;
    };
    callbacks.open_item_actions = []() { (void)notes_page_runtime::ShowItemActionsModal(); };
    notes_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplyNotesMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyNotesFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

bool ResolveNotesTouchTarget(int x, int y, app_interaction::InteractiveTarget* target, void*)
{
    return notes_page_runtime::ResolveTouchTarget(x, y, target);
}

bool FocusNotesTouchTarget(const app_interaction::InteractiveTarget& target, void*)
{
    const page_actions::FocusUpdateOutcome outcome = notes_page_runtime::FocusTouchTarget(target);
    ApplyNotesFocusUpdate(outcome);
    return outcome.handled;
}

bool FocusNotesFooterTarget(const app_interaction::InteractiveTarget& target)
{
    const page_actions::FocusUpdateOutcome outcome =
        notes_page_runtime::FocusFooterItem(FooterItemFromTargetIndex(target.primary_index));
    ApplyNotesFocusUpdate(outcome);
    return outcome.handled;
}

app_interaction::InputResult ActivateNotesTouchTarget(
    const app_interaction::InteractiveTarget& target, void*)
{
    const ButtonResult result =
        ApplyNotesActivateResult(notes_page_runtime::ActivateTouchTarget(target));
    return result.interaction_result;
}

ButtonResult HandleNotesButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: DOWN double-click exits an entered item list.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kDoubleClick) {
        if (notes_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyNotesActivateResult(notes_page_runtime::ActivateFocusedItem());
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
            return {
                .resolve_touch_target = &ResolveDashboardTouchTarget,
                .focus_touch_target = &FocusDashboardTouchTarget,
                .activate_touch_target = &ActivateDashboardTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kVibeCheck:
            return {
                .resolve_touch_target = &ResolveVibeCheckTouchTarget,
                .focus_touch_target = &FocusVibeCheckTouchTarget,
                .activate_touch_target = &ActivateVibeCheckTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kSummarize:
            return {
                .resolve_touch_target = &ResolveSummarizeTouchTarget,
                .focus_touch_target = &FocusSummarizeTouchTarget,
                .activate_touch_target = &ActivateSummarizeTouchTarget,
                .context = nullptr,
            };
        case display_service::ScreenId::kNotes:
            return {
                .resolve_touch_target = &ResolveNotesTouchTarget,
                .focus_touch_target = &FocusNotesTouchTarget,
                .activate_touch_target = &ActivateNotesTouchTarget,
                .context = nullptr,
            };
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
            // App-wide gesture: DOWN double-click exits an entered UI (here, the network list).
            if (event.button != button_service::ButtonId::kDown) {
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

ButtonResult HandleDashboardButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};
    if (event.button != button_service::ButtonId::kPowerOk) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyDashboardActivateResult(dashboard_page_runtime::ActivateFocusedItem());
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
            return dashboard_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kVibeCheck:
            return vibe_check_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kSummarize:
            return summarize_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kNotes:
            return notes_page_runtime::BuildFooterProjectionState();
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
            dashboard_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kVibeCheck:
            vibe_check_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kSummarize:
            summarize_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kNotes:
            notes_page_runtime::ResetFocus();
            return;
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
            return FocusDashboardFooterTarget(target);
        case display_service::ScreenId::kVibeCheck:
            return FocusVibeCheckFooterTarget(target);
        case display_service::ScreenId::kSummarize:
            return FocusSummarizeFooterTarget(target);
        case display_service::ScreenId::kNotes:
            return FocusNotesFooterTarget(target);
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
            return ApplyDashboardMoveResult(dashboard_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kVibeCheck:
            return ApplyVibeCheckMoveResult(vibe_check_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kSummarize:
            return ApplySummarizeMoveResult(summarize_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kNotes:
            return ApplyNotesMoveResult(notes_page_runtime::MoveFocus(delta));
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
            return HandleDashboardButtonEvent(event);
        case display_service::ScreenId::kVibeCheck:
            return HandleVibeCheckButtonEvent(event);
        case display_service::ScreenId::kSummarize:
            return HandleSummarizeButtonEvent(event);
        case display_service::ScreenId::kNotes:
            return HandleNotesButtonEvent(event);
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

}  // namespace page_input_runtime
