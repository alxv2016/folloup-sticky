#include "page_input_runtime.h"

#include "epaper_ui/toast.h"

#include "dashboard_page_interactions.h"
#include "dashboard_page_runtime.h"
#include "details_page_interactions.h"
#include "details_page_runtime.h"
#include "follow_up_page_interactions.h"
#include "follow_up_page_runtime.h"
#include "notes_page_interactions.h"
#include "notes_page_runtime.h"
#include "onboarding_page_interactions.h"
#include "onboarding_page_runtime.h"
#include "overlay_runtime.h"
#include "todos_page_interactions.h"
#include "todos_page_runtime.h"
#include "settings_page_interactions.h"
#include "settings_page_runtime.h"
#include "recording_session_service.h"
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
    callbacks.enable_otg = []() {
        // The button is a toggle: while OTG is up it exits, otherwise it enters. Entering
        // hands the SD card to the host, so it is refused mid-recording.
        if (storage_service::IsUsbModeActive()) {
            (void)storage_service::RequestExitUsbMode();
            return;
        }
        const recording_session_service::Snapshot session =
            recording_session_service::GetSnapshot();
        if (session.phase != recording_session_service::Phase::kIdle &&
            session.phase != recording_session_service::Phase::kComplete &&
            session.phase != recording_session_service::Phase::kFailed) {
            (void)overlay_runtime::ShowToastForDuration(
                []() {
                    epaper_ui::ToastState toast = {};
                    toast.visible = true;
                    toast.body_text = "Finish the recording first";
                    return toast;
                }(),
                2000);
            return;
        }
        const esp_err_t err = storage_service::RequestEnterUsbMode();
        if (err == ESP_ERR_INVALID_ARG) {
            (void)overlay_runtime::ShowStorageModalUsbNoCable();
        } else if (err == ESP_ERR_NOT_FOUND) {
            (void)overlay_runtime::ShowStorageModalNoSdCard();
        } else if (err != ESP_OK) {
            (void)overlay_runtime::ShowStorageModalUsbError();
        }
    };
    callbacks.show_onboarding = []() {
        // Deferred so the screen change happens after input dispatch; app_shell polls for it.
        onboarding_page_runtime::RequestManualLaunch();
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

ButtonResult HandleVibeCheckButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // No dedicated back key: holding DOWN leaves the card's action row.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (vibe_check_page_runtime::ExitFocusedCard()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
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

ButtonResult HandleSummarizeButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: holding DOWN exits an entered control (segment / scroll).
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (summarize_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
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

ButtonResult HandleNotesButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: holding DOWN exits an entered item list.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (notes_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
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

// --- Todos page ------------------------------------------------------------

esp_err_t ApplyTodosPageAndFooterDisplayState()
{
    const esp_err_t page_err = todos_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyTodosPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)todos_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplyTodosFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(todos_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kTodosPage,
                                               &ApplyTodosPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplyTodosPageStateUpdate(refresh_request);
    }
}

ButtonResult ApplyTodosActivateResult(const todos_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    if (activation.apply_page_state) {
        ApplyTodosPageStateUpdate({
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        });
    }

    todos_page_interactions::ActivateCallbacks callbacks = {};
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
    callbacks.open_item_actions = []() { (void)todos_page_runtime::ShowItemActionsModal(); };
    todos_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplyTodosMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyTodosFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

ButtonResult HandleTodosButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: holding DOWN exits an entered item list.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (todos_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyTodosActivateResult(todos_page_runtime::ActivateFocusedItem());
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

// --- Follow-up page --------------------------------------------------------

esp_err_t ApplyFollowUpPageAndFooterDisplayState()
{
    const esp_err_t page_err = follow_up_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyFollowUpPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)follow_up_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplyFollowUpFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(follow_up_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kFollowUpPage,
                                               &ApplyFollowUpPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplyFollowUpPageStateUpdate(refresh_request);
    }
}

ButtonResult ApplyFollowUpActivateResult(
    const follow_up_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    if (activation.apply_page_state) {
        ApplyFollowUpPageStateUpdate({
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        });
    }

    follow_up_page_interactions::ActivateCallbacks callbacks = {};
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
    callbacks.open_item_actions = []() { (void)follow_up_page_runtime::ShowItemActionsModal(); };
    follow_up_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplyFollowUpMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyFollowUpFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

ButtonResult HandleFollowUpButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: holding DOWN exits an entered item list.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (follow_up_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyFollowUpActivateResult(follow_up_page_runtime::ActivateFocusedItem());
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

// --- Onboarding page -------------------------------------------------------

ButtonResult ApplyOnboardingActivateResult(
    const onboarding_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    // Prev/Next change the slide in place inside HandlePrimaryActivate; repaint here.
    if (activation.apply_page_state) {
        (void)onboarding_page_runtime::UpdateDisplayStateAndRequestRefresh(
            display_service::RefreshMode::kPartial);
    }

    onboarding_page_interactions::ActivateCallbacks callbacks = {};
    // Close is deferred so the screen change happens after input dispatch returns.
    callbacks.dismiss = []() { onboarding_page_runtime::RequestDismiss(); };
    onboarding_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    return result;
}

FocusMoveResult ApplyOnboardingMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    if (outcome.apply_page_state) {
        (void)onboarding_page_runtime::UpdateDisplayStateAndRequestRefresh(
            display_service::RefreshMode::kPartial);
    }
    return result;
}

ButtonResult HandleOnboardingButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // Consistent with every other page: UP/DOWN rove (handled upstream in input_focus_runtime),
    // POWER_OK activates the focused control (Prev/Next change the slide, Close dismisses).
    if (!button_service::IsPrimaryButton(event.button)) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyOnboardingActivateResult(onboarding_page_runtime::ActivateFocusedItem());
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

// --- Details page ----------------------------------------------------------

esp_err_t ApplyDetailsPageAndFooterDisplayState()
{
    const esp_err_t page_err = details_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        return page_err;
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        return footer_err;
    }
    return page_err != ESP_OK ? page_err : footer_err;
}

void ApplyDetailsPageStateUpdate(const display_service::RefreshRequest& refresh_request)
{
    (void)details_page_runtime::UpdateDisplayStateAndRequestRefresh(refresh_request);
}

void ApplyDetailsFocusUpdate(const page_actions::FocusUpdateOutcome& outcome)
{
    if (!outcome.handled) {
        return;
    }
    if (outcome.sync_footer_projection) {
        footer_runtime::SetProjectionState(details_page_runtime::BuildFooterProjectionState());
    }
    if (outcome.apply_page_state) {
        const display_service::RefreshRequest refresh_request = {
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        };
        if (outcome.sync_footer_projection) {
            (void)ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kDetailsPage,
                                               &ApplyDetailsPageAndFooterDisplayState,
                                               refresh_request);
            return;
        }
        ApplyDetailsPageStateUpdate(refresh_request);
    }
}

ButtonResult ApplyDetailsActivateResult(const details_page_interactions::ActivateResult& activation)
{
    ButtonResult result = {};
    if (!activation.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(activation.play_activate_cue);

    // Entering the scroll container mutates page state inside HandlePrimaryActivate; repaint here.
    if (activation.apply_page_state) {
        ApplyDetailsPageStateUpdate({
            .refresh_mode = display_service::RefreshMode::kPartial,
            .scope = display_service::RefreshScope::kRegion,
        });
    }

    details_page_interactions::ActivateCallbacks callbacks = {};
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
    // Deferred so the screen change happens after input dispatch returns.
    callbacks.show_previous_page = []() { details_page_runtime::RequestBack(); };
    callbacks.transcribe = []() { details_page_runtime::RequestTranscribe(); };
    callbacks.play = []() { details_page_runtime::RequestPlay(); };
    details_page_interactions::ApplyPrimaryActivateResult(activation, callbacks);
    if (result.footer_item != footer_runtime::FooterFocusItem::kNone) {
        result.interaction_result.play_feedback = false;
        result.interaction_result.feedback_cue = app_interaction::FeedbackCue::kNone;
    }
    return result;
}

FocusMoveResult ApplyDetailsMoveResult(const page_actions::FocusMoveOutcome& outcome)
{
    FocusMoveResult result = {};
    if (!outcome.handled) {
        return result;
    }
    result.handled = true;
    result.interaction_result = MakeConsumedResult(outcome.play_navigation_cue);
    ApplyDetailsFocusUpdate({
        .handled = outcome.handled,
        .apply_page_state = outcome.apply_page_state,
        .sync_footer_projection = outcome.sync_footer_projection,
    });
    return result;
}

ButtonResult HandleDetailsButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};

    // App-wide gesture: holding DOWN exits the entered scroll container.
    if (event.button == button_service::ButtonId::kDown &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if (details_page_runtime::ExitActiveControl()) {
            result.handled = true;
            result.interaction_result = MakeConsumedResult(true);
        }
        return result;
    }

    if (!button_service::IsPrimaryButton(event.button)) {
        return result;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            return ApplyDetailsActivateResult(details_page_runtime::ActivateFocusedItem());
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

ButtonResult HandleSettingsButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};
    if (!button_service::IsPrimaryButton(event.button)) {
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
            if (!button_service::IsPrimaryButton(event.button)) {
                return result;
            }
            return ApplyWifiActivateResult(wifi_page_runtime::ActivateFocusedItem());
        case button_service::ButtonEvent::kLongPressStart:
            // App-wide gesture: holding DOWN exits an entered UI (here, the network list).
            if (event.button == button_service::ButtonId::kDown) {
                return ApplyWifiSecondaryActivateResult(
                    wifi_page_runtime::SecondaryActivateFocusedItem());
            }
            if (!button_service::IsPrimaryButton(event.button)) {
                return result;
            }
            result.handled = true;
            result.interaction_result.consumed = true;
            return result;
        case button_service::ButtonEvent::kPressDown:
        case button_service::ButtonEvent::kPressUp:
        case button_service::ButtonEvent::kPressRepeat:
        case button_service::ButtonEvent::kLongPressUp:
            if (!button_service::IsPrimaryButton(event.button)) {
                return result;
            }
            result.handled = true;
            result.interaction_result.consumed = true;
            return result;
        case button_service::ButtonEvent::kDoubleClick:
        default:
            return result;
    }
}

ButtonResult HandleTimeButtonEvent(const button_service::ButtonEventInfo& event)
{
    ButtonResult result = {};
    if (!button_service::IsPrimaryButton(event.button)) {
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
    if (!button_service::IsPrimaryButton(event.button)) {
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
        case display_service::ScreenId::kTodos:
            return todos_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kFollowUp:
            return follow_up_page_runtime::BuildFooterProjectionState();
        case display_service::ScreenId::kDetails:
            return details_page_runtime::BuildFooterProjectionState();
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
        case display_service::ScreenId::kTodos:
            todos_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kFollowUp:
            follow_up_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kDetails:
            details_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kOnboarding:
            onboarding_page_runtime::ResetFocus();
            return;
        case display_service::ScreenId::kLockScreen:
        default:
            return;
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
        case display_service::ScreenId::kTodos:
            return ApplyTodosMoveResult(todos_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kFollowUp:
            return ApplyFollowUpMoveResult(follow_up_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kDetails:
            return ApplyDetailsMoveResult(details_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kOnboarding:
            return ApplyOnboardingMoveResult(onboarding_page_runtime::MoveFocus(delta));
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

ButtonResult HandleButtonEventForScreen(display_service::ScreenId screen,
                                        const button_service::ButtonEventInfo& event)
{
    switch (screen) {
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
        case display_service::ScreenId::kTodos:
            return HandleTodosButtonEvent(event);
        case display_service::ScreenId::kFollowUp:
            return HandleFollowUpButtonEvent(event);
        case display_service::ScreenId::kDetails:
            return HandleDetailsButtonEvent(event);
        case display_service::ScreenId::kOnboarding:
            return HandleOnboardingButtonEvent(event);
        case display_service::ScreenId::kLockScreen:
        default:
            return {};
    }
}

ButtonResult HandleButtonEventForCurrentScreen(const button_service::ButtonEventInfo& event)
{
    const display_service::ScreenId screen = display_service::GetCurrentScreen();
    ButtonResult result = HandleButtonEventForScreen(screen, event);

    // The Sticky footer button is uniform across pages (always opens the sticky overlay), so rather
    // than thread a per-page intent through every page's activation, handle it centrally: the shared
    // per-page footer handler only maps Home/Settings/Wifi/Time, so a focused Sticky button falls
    // through here on a POWER_OK single click.
    if (!result.handled && button_service::IsPrimaryButton(event.button) &&
        event.event == button_service::ButtonEvent::kSingleClick &&
        BuildFooterProjectionForScreen(screen).focused_item ==
            footer_runtime::FooterFocusItem::kSticky) {
        result.handled = true;
        result.interaction_result = MakeConsumedResult(true);
        result.footer_item = footer_runtime::FooterFocusItem::kSticky;
    }
    return result;
}

}  // namespace page_input_runtime
