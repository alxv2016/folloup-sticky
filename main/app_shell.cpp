#include "app_shell.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "button_input_runtime.h"
#include "button_service.h"
#include "device_sleep_service.h"
#include "device_sleep_runtime.h"
#include "display_service.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "feedback_service.h"
#include "footer_runtime.h"
#include "followup_task_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gemini_service.h"
#include "imu_service.h"
#include "input_focus_runtime.h"
#include "input_runtime_setup.h"
#include "lock_screen_runtime.h"
#include "overlay_runtime.h"
#include "page_input_runtime.h"
#include "power_service.h"
#include "project_assets.h"
#include "recording_session_service.h"
#include "recording_service.h"
#include "sdkconfig.h"
#include "settings_page_runtime.h"
#include "details_page_runtime.h"
#include "follow_up_page_runtime.h"
#include "notes_page_runtime.h"
#include "nvs.h"
#include "onboarding_page_runtime.h"
#include "status_bar_runtime.h"
#include "todos_page_runtime.h"
#include "storage_service.h"
#include "summarize_page_runtime.h"
#include "summary_service.h"
#include "timezone_service.h"
#include "transcription_service.h"
#include "ui_refresh_runtime.h"
#include "wifi_service.h"
#include "dashboard_page_runtime.h"
#include "recording_archive_service.h"
#include "timeline_format.h"
#include "time_page_runtime.h"
#include "vibe_check_page_runtime.h"
#include "wifi_page_runtime.h"

namespace app_shell {
namespace {

constexpr const char* kTag = "AppShell";
constexpr bool kEnablePowerButtonShutdown = true;
constexpr uint32_t kAutoSleepDisplaySleepTimeoutSeconds =
    CONFIG_FOLLOWUP_AUTO_SLEEP_DISPLAY_SLEEP_TIMEOUT_SECONDS;
constexpr uint32_t kAutoSleepLightSleepTimeoutSeconds =
    CONFIG_FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS;
constexpr uint32_t kShutdownTaskStackWords = 3072;
constexpr TickType_t kPowerButtonReleaseSettleDelay = pdMS_TO_TICKS(500);

TaskHandle_t s_shutdown_task = nullptr;
std::atomic<bool> s_startup_complete = false;
std::atomic<bool> s_gemini_ready = false;
std::mutex s_recording_session_feedback_mutex;
recording_session_service::Phase s_last_recording_session_feedback_phase =
    recording_session_service::Phase::kIdle;
std::string s_last_recording_session_feedback_status = {};

std::mutex s_summary_feedback_mutex;
bool s_summary_feedback_seen = false;
uint32_t s_last_summary_feedback_generation = 0;

void PlayFeedback(feedback_service::FeedbackEvent event)
{
    (void)feedback_service::Play(event);
}

app_interaction::InputResult MakeFeedbackResult(app_interaction::FeedbackCue cue)
{
    app_interaction::InputResult result = {};
    result.play_feedback = true;
    result.feedback_cue = cue;
    return result;
}

void PlayInteractionFeedback(const app_interaction::InputResult& result)
{
    if (!result.play_feedback) {
        return;
    }

    switch (result.feedback_cue) {
        case app_interaction::FeedbackCue::kClick:
            PlayFeedback(feedback_service::FeedbackEvent::kButtonClick);
            break;
        case app_interaction::FeedbackCue::kTouchContact:
            PlayFeedback(feedback_service::FeedbackEvent::kTouchContact);
            break;
        case app_interaction::FeedbackCue::kModalOpen:
            PlayFeedback(feedback_service::FeedbackEvent::kModalOpen);
            break;
        case app_interaction::FeedbackCue::kError:
            PlayFeedback(feedback_service::FeedbackEvent::kError);
            break;
        case app_interaction::FeedbackCue::kRecordingStart:
            PlayFeedback(feedback_service::FeedbackEvent::kRecordingStart);
            break;
        case app_interaction::FeedbackCue::kNone:
        default:
            break;
    }
}

bool InputsEnabled(void*)
{
    return s_startup_complete.load(std::memory_order_relaxed);
}

void FlushOverlayFeedback()
{
    app_interaction::FeedbackCue cue = app_interaction::FeedbackCue::kNone;
    while (overlay_runtime::TakePendingFeedback(&cue)) {
        PlayInteractionFeedback(MakeFeedbackResult(cue));
    }
}

void SyncStatusBarState(const char* source)
{
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update before display refresh from %s failed: %s",
                 source, esp_err_to_name(status_bar_err));
    }
}

// A page may only request its own partial refresh once boot has painted the initial
// full screen. Before s_startup_complete, ShowHomeScreen(kFull) at the end of Run()
// repaints everything, so any earlier partial is redundant work — and ghosts on a
// freshly-powered panel that has no full-flush baseline yet. Folding the startup gate
// into the "is this screen active" check keeps every event handler from firing a
// boot-time partial.
bool ScreenActiveForRefresh(display_service::ScreenId screen)
{
    return s_startup_complete.load(std::memory_order_relaxed) &&
           display_service::GetCurrentScreen() == screen;
}

footer_runtime::LayoutState FooterLayoutForScreen(display_service::ScreenId screen)
{
    footer_runtime::LayoutState layout = {};
    layout.visible = true;
    layout.show_settings = true;
    layout.show_wifi = true;
    layout.show_time = true;
    // Home button is always visible, including on the home screen itself (tapping it
    // there does a full-screen refresh via HandleFooterActivate -> ShowHomeScreen(kFull)).
    layout.show_home = true;
    // Sticky button sits left of Home; it opens the follow-up sticky-note overlay from any page.
    layout.show_sticky = true;
    layout.show_mic = true;
    return layout;
}

esp_err_t SyncSettingsPageState(bool request_refresh_if_active)
{
    const bool active = ScreenActiveForRefresh(display_service::ScreenId::kSettings);
    const esp_err_t err =
        request_refresh_if_active && active
            ? settings_page_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : settings_page_runtime::UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Settings page state sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t SyncWifiPageState(bool request_refresh_if_active)
{
    const bool active = ScreenActiveForRefresh(display_service::ScreenId::kWifi);
    const esp_err_t err = wifi_page_runtime::SyncFromService(request_refresh_if_active && active);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "WiFi page state sync failed: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t ShowHomeScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_home_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kHome);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kHome));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kHome));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before home screen failed: %s", esp_err_to_name(footer_err));
    }
    const esp_err_t dashboard_err = dashboard_page_runtime::SyncFromService(false);
    if (dashboard_err != ESP_OK && dashboard_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Dashboard sync before home screen failed: %s",
                 esp_err_to_name(dashboard_err));
    }
    // Reconcile the archive counts against the SD card once, off the boot path: the dashboard
    // rendered from the NVS-cached snapshot above; this background scan repaints only if the
    // counts actually changed (e.g. first boot, or the card was edited externally).
    static std::atomic<bool> s_archive_reconcile_started{false};
    if (!s_archive_reconcile_started.exchange(true)) {
        recording_archive_service::RefreshAsync();
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kHome, refresh_mode,
                                             "show_home_screen");
}

esp_err_t ShowSettingsScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_settings_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kSettings);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kSettings));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kSettings));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before settings screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    const esp_err_t settings_err = settings_page_runtime::UpdateDisplayState();
    if (settings_err != ESP_OK && settings_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Settings page sync before show failed: %s",
                 esp_err_to_name(settings_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kSettings, refresh_mode,
                                             "show_settings_screen");
}

esp_err_t ShowVibeCheckScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_vibe_check_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kVibeCheck);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kVibeCheck));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kVibeCheck));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before vibe check screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    const esp_err_t vibe_err = vibe_check_page_runtime::SyncFromService(false);
    if (vibe_err != ESP_OK && vibe_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Vibe check page sync before show failed: %s", esp_err_to_name(vibe_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kVibeCheck, refresh_mode,
                                             "show_vibe_check_screen");
}

void HandleSummaryEvent(const summary_service::Event& event, void* context);

esp_err_t ShowSummarizeScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_summarize_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kSummarize);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kSummarize));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kSummarize));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before summarize screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    // Lazily start the summary service (seeds its cache from SD) and subscribe for updates.
    (void)summary_service::Init();
    summary_service::SetEventHandler(HandleSummaryEvent, nullptr);
    const esp_err_t sync_err = summarize_page_runtime::SyncFromService(false);
    if (sync_err != ESP_OK && sync_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Summarize page sync before show failed: %s", esp_err_to_name(sync_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kSummarize, refresh_mode,
                                             "show_summarize_screen");
}

esp_err_t ShowNotesScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_notes_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kNotes);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kNotes));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kNotes));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before notes screen failed: %s", esp_err_to_name(footer_err));
    }
    // Build the timeline from the archive (SD read) before showing.
    const esp_err_t sync_err = notes_page_runtime::SyncFromArchive(false);
    if (sync_err != ESP_OK && sync_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Notes page sync before show failed: %s", esp_err_to_name(sync_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kNotes, refresh_mode,
                                             "show_notes_screen");
}

esp_err_t ShowTodosScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_todos_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kTodos);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kTodos));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kTodos));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before todos screen failed: %s", esp_err_to_name(footer_err));
    }
    // Build the timeline from the archive (SD read) before showing.
    const esp_err_t sync_err = todos_page_runtime::SyncFromArchive(false);
    if (sync_err != ESP_OK && sync_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Todos page sync before show failed: %s", esp_err_to_name(sync_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kTodos, refresh_mode,
                                             "show_todos_screen");
}

esp_err_t ShowFollowUpScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_follow_up_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kFollowUp);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kFollowUp));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kFollowUp));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before follow-up screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    // Build the timeline from the archive (SD read) before showing.
    const esp_err_t sync_err = follow_up_page_runtime::SyncFromArchive(false);
    if (sync_err != ESP_OK && sync_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Follow-up page sync before show failed: %s", esp_err_to_name(sync_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kFollowUp, refresh_mode,
                                             "show_follow_up_screen");
}

// Persisted first-run flag. Device/firmware state (not tied to the SD card), so it survives an SD
// format; a future Settings action can clear it to replay onboarding.
constexpr const char* kOnboardingNvsNamespace = "app_state";
constexpr const char* kOnboardingNvsKey = "onboarded";

// True while onboarding was opened via the Settings "Manual" button (as opposed to first boot). In
// that mode dismissal returns to Settings and does NOT touch the "onboarded" flag.
bool s_onboarding_from_settings = false;

bool OnboardingViewed()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kOnboardingNvsNamespace, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t viewed = 0;
    const esp_err_t err = nvs_get_u8(handle, kOnboardingNvsKey, &viewed);
    nvs_close(handle);
    return err == ESP_OK && viewed != 0;
}

void MarkOnboardingViewed()
{
    nvs_handle_t handle = 0;
    if (nvs_open(kOnboardingNvsNamespace, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(kTag, "Onboarding flag: nvs_open failed");
        return;
    }
    if (nvs_set_u8(handle, kOnboardingNvsKey, 1) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

esp_err_t ShowOnboardingScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_onboarding_screen");
    // The carousel's own control row replaces the footer. Hide the footer's layout so its touch
    // layer stops intercepting taps in the bottom band (where the carousel controls live).
    footer_runtime::LayoutState hidden_footer = {};
    hidden_footer.visible = false;
    hidden_footer.show_mic = false;
    footer_runtime::SetLayoutState(hidden_footer);
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kOnboarding);
    const esp_err_t page_err = onboarding_page_runtime::UpdateDisplayState();
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Onboarding page state build failed: %s", esp_err_to_name(page_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kOnboarding, refresh_mode,
                                             "show_onboarding_screen");
}

// Open onboarding from the Settings "Manual" button, deferred out of input dispatch. Does not touch
// the "onboarded" flag; dismissal returns to Settings (see HandleOnboardingDismissIfRequested).
void ShowOnboardingFromSettingsIfRequested()
{
    if (!onboarding_page_runtime::ConsumePendingManualLaunch()) {
        return;
    }
    s_onboarding_from_settings = true;
    const esp_err_t err = ShowOnboardingScreen(display_service::RefreshMode::kFull);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Manual onboarding launch failed: %s", esp_err_to_name(err));
    }
}

// Finish onboarding, deferred out of input dispatch. First-run onboarding persists the flag and
// goes to the dashboard; a manual launch from Settings just returns to Settings, flag untouched.
void HandleOnboardingDismissIfRequested()
{
    if (!onboarding_page_runtime::ConsumePendingDismiss()) {
        return;
    }
    if (s_onboarding_from_settings) {
        s_onboarding_from_settings = false;
        const esp_err_t err = ShowSettingsScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Onboarding dismiss -> settings failed: %s", esp_err_to_name(err));
        }
        return;
    }
    MarkOnboardingViewed();
    const esp_err_t err = ShowHomeScreen(display_service::RefreshMode::kFull);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Onboarding dismiss -> home failed: %s", esp_err_to_name(err));
    }
}

esp_err_t ShowDetailsScreen(const std::string& recording_id, DetailsPageSource source,
                            display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_details_screen");
    details_page_runtime::QueueShow(recording_id, source);
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kDetails);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kDetails));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kDetails));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before details screen failed: %s", esp_err_to_name(footer_err));
    }
    // Load the recording from the archive (SD read) before showing.
    const esp_err_t sync_err = details_page_runtime::SyncFromArchive(false);
    if (sync_err != ESP_OK && sync_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Details page sync before show failed: %s", esp_err_to_name(sync_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kDetails, refresh_mode,
                                             "show_details_screen");
}

// Open the Details page for a recording requested via the Notes item-actions modal.
void ShowDetailsScreenIfRequested()
{
    std::string recording_id = notes_page_runtime::ConsumePendingViewDetails();
    DetailsPageSource source = DetailsPageSource::kNotes;
    if (recording_id.empty()) {
        recording_id = todos_page_runtime::ConsumePendingViewDetails();
        source = DetailsPageSource::kTodos;
    }
    if (recording_id.empty()) {
        recording_id = follow_up_page_runtime::ConsumePendingViewDetails();
        source = DetailsPageSource::kFollowUp;
    }
    if (recording_id.empty()) {
        return;
    }
    const esp_err_t err =
        ShowDetailsScreen(recording_id, source, display_service::RefreshMode::kFull);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Show details screen failed: %s", esp_err_to_name(err));
    }
}

// Return from the Details page to whichever page opened it.
void HandleDetailsBackIfRequested()
{
    if (!details_page_runtime::ConsumePendingBack()) {
        return;
    }
    esp_err_t err = ESP_OK;
    switch (details_page_runtime::SourcePage()) {
        case DetailsPageSource::kNotes:
            err = ShowNotesScreen(display_service::RefreshMode::kFull);
            break;
        case DetailsPageSource::kTodos:
            err = ShowTodosScreen(display_service::RefreshMode::kFull);
            break;
        case DetailsPageSource::kFollowUp:
            err = ShowFollowUpScreen(display_service::RefreshMode::kFull);
            break;
        default:
            err = ShowHomeScreen(display_service::RefreshMode::kFull);
            break;
    }
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Details back navigation failed: %s", esp_err_to_name(err));
    }
}

esp_err_t ShowWifiScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_wifi_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kWifi);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kWifi));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kWifi));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before WiFi screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    const esp_err_t wifi_err = wifi_page_runtime::SyncFromService(false);
    if (wifi_err != ESP_OK && wifi_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "WiFi page sync before show failed: %s", esp_err_to_name(wifi_err));
    }
    // Kick off a scan on entry so the list fills with all nearby networks (not just
    // the connected one, which is all the empty snapshot yields). The scan is async;
    // when it completes wifi_service fires an event -> HandleWifiEvent -> the page
    // re-syncs with the results. Safe to call unconditionally: StartNetworkScan is a
    // no-op when WiFi is disabled or a scan is already running.
    (void)wifi_service::StartNetworkScan();
    return display_service::SetCurrentScreen(display_service::ScreenId::kWifi, refresh_mode,
                                             "show_wifi_screen");
}

esp_err_t ShowTimeScreen(display_service::RefreshMode refresh_mode)
{
    SyncStatusBarState("show_time_screen");
    page_input_runtime::ResetFocusForScreen(display_service::ScreenId::kTime);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kTime));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kTime));
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer sync before time screen failed: %s",
                 esp_err_to_name(footer_err));
    }
    const esp_err_t time_err = time_page_runtime::SyncFromService(false);
    if (time_err != ESP_OK && time_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Time page sync before show failed: %s", esp_err_to_name(time_err));
    }
    return display_service::SetCurrentScreen(display_service::ScreenId::kTime, refresh_mode,
                                             "show_time_screen");
}

// Gather every follow-up recording (newest first) as sticky-note items, reusing the same content
// shape as the Follow-up timeline rows.
std::vector<overlay_runtime::StickyNoteItem> BuildFollowUpStickyItems()
{
    esp_err_t status = ESP_OK;
    const std::vector<recording_archive_service::RecordingEntry> entries =
        recording_archive_service::ListRecordings(&status);

    std::vector<const recording_archive_service::RecordingEntry*> follow_ups;
    for (const recording_archive_service::RecordingEntry& entry : entries) {
        if (entry.metadata.follow_up) {
            follow_ups.push_back(&entry);
        }
    }
    const auto entry_time = [](const recording_archive_service::RecordingEntry* entry) {
        return entry->metadata.created_unix_seconds > 0 ? entry->metadata.created_unix_seconds
                                                        : entry->modified_unix_seconds;
    };
    std::sort(follow_ups.begin(), follow_ups.end(),
              [&](const recording_archive_service::RecordingEntry* a,
                  const recording_archive_service::RecordingEntry* b) {
                  return entry_time(a) > entry_time(b);
              });

    std::vector<overlay_runtime::StickyNoteItem> items;
    items.reserve(follow_ups.size());
    for (const recording_archive_service::RecordingEntry* entry_ptr : follow_ups) {
        const recording_archive_service::RecordingEntry& entry = *entry_ptr;
        const std::string transcript = timeline_format::TrimTranscript(entry.transcript_text);
        const bool has_transcription = entry.metadata.has_transcript && !transcript.empty();

        overlay_runtime::StickyNoteItem item = {};
        // Recorded date on top (plain text); the tag sits in the header row, after the pin icon.
        item.date_text = timeline_format::FormatDateLabel(entry.metadata.created_local_date);
        item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        item.header.tag_icon_asset = project_assets::GetIcon(EmbeddedIconId::kPin);
        item.header.tag_text = timeline_format::TagText(entry.metadata.tag);
        item.header.time_text = timeline_format::FormatTimeLabel(
            entry.metadata.time_valid, entry.metadata.created_unix_seconds);
        item.header.minute_seconds_text =
            timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
        item.body_text = has_transcription ? transcript : "Audio only follow-up item.";
        items.push_back(std::move(item));
    }
    return items;
}

// Footer "Sticky" action: show the follow-up notes in the sticky overlay, or a nudge toast when
// there are none yet.
void ShowFollowUpStickyNotes()
{
    const std::vector<overlay_runtime::StickyNoteItem> items = BuildFollowUpStickyItems();
    if (items.empty()) {
        epaper_ui::ToastState toast = {};
        toast.visible = true;
        toast.body_text = "Follow up on a thought to view";
        toast.leading_icon = project_assets::GetIcon(EmbeddedIconId::kPin);
        (void)overlay_runtime::ShowToastForDuration(toast, 2000);
        return;
    }
    const esp_err_t err = overlay_runtime::ShowStickyNotes(items);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Show sticky notes failed: %s", esp_err_to_name(err));
    }
}

app_interaction::InputResult HandleFooterActivate(footer_runtime::FooterFocusItem item, void*)
{
    app_interaction::InputResult result = {};
    result.consumed = true;

    ESP_LOGI(kTag, "Footer activate: item=%d", static_cast<int>(item));

    esp_err_t err = ESP_OK;
    switch (item) {
        case footer_runtime::FooterFocusItem::kHome:
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            err = ShowHomeScreen(display_service::RefreshMode::kFull);
            break;
        case footer_runtime::FooterFocusItem::kSettings:
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            err = ShowSettingsScreen(display_service::RefreshMode::kFull);
            break;
        case footer_runtime::FooterFocusItem::kWifi:
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            err = ShowWifiScreen(display_service::RefreshMode::kFull);
            break;
        case footer_runtime::FooterFocusItem::kTime:
            result.play_feedback = true;
            result.feedback_cue = app_interaction::FeedbackCue::kClick;
            err = ShowTimeScreen(display_service::RefreshMode::kFull);
            break;
        case footer_runtime::FooterFocusItem::kSticky:
            // Opens the follow-up sticky overlay (or a nudge toast). The overlay owns its own
            // refresh + feedback, and there is no underlying screen change, so return directly.
            ShowFollowUpStickyNotes();
            return result;
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        case footer_runtime::FooterFocusItem::kNone:
        default:
            return result;
    }

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer activation failed for item=%d: %s",
                 static_cast<int>(item), esp_err_to_name(err));
    }
    return result;
}

// Routes a dashboard menu tap/press to its page. Returns false for items whose destination
// page does not exist yet, letting dashboard_page_runtime fall back to its "coming soon" toast.
bool HandleDashboardMenuItem(int menu_index, void*)
{
    if (menu_index == static_cast<int>(epaper_ui::DashboardMenuItem::kVibeCheck)) {
        const esp_err_t err = ShowVibeCheckScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show vibe check screen failed: %s", esp_err_to_name(err));
        }
        return true;
    }
    if (menu_index == static_cast<int>(epaper_ui::DashboardMenuItem::kSummarize)) {
        const esp_err_t err = ShowSummarizeScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show summarize screen failed: %s", esp_err_to_name(err));
        }
        return true;
    }
    if (menu_index == static_cast<int>(epaper_ui::DashboardMenuItem::kNotes)) {
        const esp_err_t err = ShowNotesScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show notes screen failed: %s", esp_err_to_name(err));
        }
        return true;
    }
    if (menu_index == static_cast<int>(epaper_ui::DashboardMenuItem::kTodos)) {
        const esp_err_t err = ShowTodosScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show todos screen failed: %s", esp_err_to_name(err));
        }
        return true;
    }
    if (menu_index == static_cast<int>(epaper_ui::DashboardMenuItem::kFollowUp)) {
        const esp_err_t err = ShowFollowUpScreen(display_service::RefreshMode::kFull);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show follow-up screen failed: %s", esp_err_to_name(err));
        }
        return true;
    }
    return false;
}

void ConfirmPendingOtaImage()
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;

    if (running != nullptr &&
        esp_ota_get_state_partition(running, &ota_state) == ESP_OK &&
        ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
    }
}

const char* ButtonIdName(button_service::ButtonId button)
{
    switch (button) {
        case button_service::ButtonId::kAction:
            return "ACTION";
        case button_service::ButtonId::kFunction:
            return "FN";
        case button_service::ButtonId::kUp:
            return "UP";
        case button_service::ButtonId::kDown:
            return "DOWN";
        default:
            return "UNKNOWN";
    }
}

const char* ButtonEventName(button_service::ButtonEvent event)
{
    switch (event) {
        case button_service::ButtonEvent::kPressDown:
            return "PRESS_DOWN";
        case button_service::ButtonEvent::kPressUp:
            return "PRESS_UP";
        case button_service::ButtonEvent::kPressRepeat:
            return "PRESS_REPEAT";
        case button_service::ButtonEvent::kSingleClick:
            return "SINGLE_CLICK";
        case button_service::ButtonEvent::kDoubleClick:
            return "DOUBLE_CLICK";
        case button_service::ButtonEvent::kLongPressStart:
            return "LONG_PRESS_START";
        case button_service::ButtonEvent::kLongPressUp:
            return "LONG_PRESS_UP";
        default:
            return "UNKNOWN";
    }
}

const char* RecordingStateName(recording_service::State state)
{
    switch (state) {
        case recording_service::State::kIdle:
            return "IDLE";
        case recording_service::State::kArmed:
            return "ARMED";
        case recording_service::State::kRecording:
            return "RECORDING";
        case recording_service::State::kClipReady:
            return "CLIP_READY";
        default:
            return "UNKNOWN";
    }
}

recording_session_service::Context BuildRecordingSessionContext()
{
    recording_session_service::Context context = {};
    context.lock_screen_active = lock_screen_runtime::IsActive();
    context.overlay_visible =
        overlay_runtime::IsShutdownModalVisible() || overlay_runtime::IsStorageModalVisible() ||
        overlay_runtime::IsSelectModalVisible() || overlay_runtime::IsKeyboardVisible();
    return context;
}

epaper_ui::SelectModalState BuildRecordingTagSelectModalState()
{
    epaper_ui::SelectModalState state = {};
    state.visible = true;
    state.title_text = "Save recording as";
    state.selected_index = 0;
    for (const auto& option : recording_session_service::TagOptions()) {
        state.items.push_back({
            .label_text = std::string(option.label_text),
        });
    }
    return state;
}

epaper_ui::ToastState BuildToast(const char* text, EmbeddedIconId icon)
{
    epaper_ui::ToastState state = {};
    state.visible = true;
    state.body_text = text != nullptr ? text : "";
    state.leading_icon = project_assets::GetIcon(icon);
    return state;
}

bool IsShutdownPending(void*)
{
    return overlay_runtime::IsShutdownPending();
}

void HandleRecordingEvent(const recording_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Recording intent: state=%s armed=%d recording=%d has_clip=%d saving=%d exporting=%d samples=%u duration_ms=%lu level=%u",
             RecordingStateName(event.state),
             event.ui_state.armed ? 1 : 0,
             event.ui_state.recording ? 1 : 0,
             event.ui_state.has_clip ? 1 : 0,
             event.ui_state.saving ? 1 : 0,
             event.ui_state.exporting ? 1 : 0,
             static_cast<unsigned>(event.ui_state.recorded_samples),
             static_cast<unsigned long>(event.ui_state.duration_ms),
             static_cast<unsigned>(event.ui_state.input_level_percent));

    recording_session_service::HandleRecordingEvent(event);

    const esp_err_t footer_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? footer_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer update after recording event failed: %s",
                 esp_err_to_name(footer_err));
    }
}

void HandleTranscriptionEvent(const transcription_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Transcription intent: ready=%d in_flight=%d http=%d status=%s error=%s chars=%u",
             event.snapshot.provider_ready ? 1 : 0,
             event.snapshot.request_in_flight ? 1 : 0,
             event.snapshot.last_http_status,
             event.snapshot.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.last_status_message.c_str(),
             event.snapshot.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.last_error_code.c_str(),
             static_cast<unsigned>(event.snapshot.last_transcript.size()));
    recording_session_service::HandleTranscriptionEvent(event);
}

void HandleRecordingSessionEvent(const recording_session_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Recording session: phase=%s allowed=%d clip_saved=%d transcript_saved=%d in_flight=%d status=%s error=%s",
             recording_session_service::PhaseName(event.snapshot.phase),
             event.snapshot.allowed ? 1 : 0,
             event.snapshot.clip_saved ? 1 : 0,
             event.snapshot.transcript_saved ? 1 : 0,
             event.snapshot.request_in_flight ? 1 : 0,
             event.snapshot.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.last_status_message.c_str(),
             event.snapshot.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.last_error_code.c_str());

    bool should_emit_feedback = false;
    {
        std::lock_guard<std::mutex> lock(s_recording_session_feedback_mutex);
        should_emit_feedback =
            event.snapshot.phase != s_last_recording_session_feedback_phase ||
            event.snapshot.last_status_message != s_last_recording_session_feedback_status;
        if (should_emit_feedback) {
            s_last_recording_session_feedback_phase = event.snapshot.phase;
            s_last_recording_session_feedback_status = event.snapshot.last_status_message;
        }
    }
    if (!should_emit_feedback) {
        return;
    }

    switch (event.snapshot.phase) {
        case recording_session_service::Phase::kRecording:
            PlayInteractionFeedback(
                MakeFeedbackResult(app_interaction::FeedbackCue::kRecordingStart));
            break;
        case recording_session_service::Phase::kAwaitingTagSelection: {
            time_page_runtime::ClearPendingSelectModal();
            const esp_err_t err =
                overlay_runtime::ShowSelectModal(BuildRecordingTagSelectModalState());
            FlushOverlayFeedback();
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording select modal failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kTranscribing: {
            const esp_err_t err = overlay_runtime::ShowToast(
                BuildToast("Transcribing recording...", EmbeddedIconId::kTranscribe));
            FlushOverlayFeedback();
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show transcription toast failed: %s", esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kComplete: {
            epaper_ui::ToastState toast = {};
            if (event.snapshot.transcript_saved) {
                toast = BuildToast("Transcript saved to SD", EmbeddedIconId::kFileTranscript);
            } else if (!event.snapshot.last_error_code.empty()) {
                // Transcription was attempted but failed. Surface it as a failure (the recording
                // itself is still on SD) with a specific message for a quota/rate-limit error.
                const bool quota_exceeded =
                    event.snapshot.last_error_code == "RESOURCE_EXHAUSTED";
                toast = BuildToast(quota_exceeded ? "Gemini quota exceeded" : "Transcription failed",
                                   EmbeddedIconId::kClose);
            } else if (event.snapshot.clip_saved) {
                toast = BuildToast("Recording saved to SD", EmbeddedIconId::kCheck);
            } else {
                toast = BuildToast(event.snapshot.last_status_message.c_str(), EmbeddedIconId::kClose);
            }
            const esp_err_t err = overlay_runtime::ShowToastForDuration(toast, 2500);
            FlushOverlayFeedback();
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording completion toast failed: %s",
                         esp_err_to_name(err));
            }
            // A saved transcript flips the recording's has_transcript flag, which fires an archive
            // event; HandleRecordingArchiveEvent re-syncs whichever page is on screen (so the card
            // swaps "audio only" for the transcript). No page-specific reload is needed here.
            break;
        }
        case recording_session_service::Phase::kFailed: {
            const char* text = event.snapshot.last_status_message.empty()
                                   ? "Recording failed"
                                   : event.snapshot.last_status_message.c_str();
            const esp_err_t err = overlay_runtime::ShowToastForDuration(
                BuildToast(text, EmbeddedIconId::kClose), 2500);
            FlushOverlayFeedback();
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show recording failure toast failed: %s",
                         esp_err_to_name(err));
            }
            break;
        }
        case recording_session_service::Phase::kIdle:
        case recording_session_service::Phase::kArmed:
        case recording_session_service::Phase::kSaving:
        default:
            break;
    }
}

void HandleSummaryEvent(const summary_service::Event& event, void*)
{
    const summary_service::Snapshot& snapshot = event.snapshot;
    const summary_service::RequestSnapshot& request = snapshot.request;

    // Keep the summarize page's cached snapshot fresh; refresh it live only when it's on screen.
    const bool page_active = ScreenActiveForRefresh(display_service::ScreenId::kSummarize);
    (void)summarize_page_runtime::OnSummarySnapshot(snapshot, page_active);

    // Toast once per request transition (request_generation bumps on start/success/failure).
    bool new_transition = false;
    {
        std::lock_guard<std::mutex> lock(s_summary_feedback_mutex);
        new_transition =
            !s_summary_feedback_seen || snapshot.request_generation != s_last_summary_feedback_generation;
        if (new_transition) {
            s_summary_feedback_seen = true;
            s_last_summary_feedback_generation = snapshot.request_generation;
        }
    }
    if (!new_transition) {
        return;
    }

    esp_err_t err = ESP_OK;
    switch (request.phase) {
        case summary_service::RequestPhase::kStarted:
            err = overlay_runtime::ShowToast(
                BuildToast(request.status_message.c_str(), EmbeddedIconId::kTranscribe));
            break;
        case summary_service::RequestPhase::kSucceeded:
            err = overlay_runtime::ShowToastForDuration(
                BuildToast(request.status_message.c_str(), EmbeddedIconId::kCheck), 2500);
            break;
        case summary_service::RequestPhase::kFailed: {
            const char* text =
                request.status_message.empty() ? "Summary failed" : request.status_message.c_str();
            err = overlay_runtime::ShowToastForDuration(BuildToast(text, EmbeddedIconId::kClose),
                                                        2500);
            break;
        }
        case summary_service::RequestPhase::kIdle:
        default:
            break;
    }
    FlushOverlayFeedback();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Show summary toast failed: %s", esp_err_to_name(err));
    }
}

void HandleStorageEvent(const storage_service::Event& event, void*)
{
    ESP_LOGI(kTag, "Storage intent: mode=%s operation=%s phase=%s err=%s",
             storage_service::ModeName(event.snapshot.mode),
             storage_service::OperationName(event.snapshot.operation),
             storage_service::OperationPhaseName(event.snapshot.phase),
             esp_err_to_name(event.snapshot.last_error));

    if (event.snapshot.operation == storage_service::Operation::kFormatSd) {
        esp_err_t overlay_err = ESP_OK;
        switch (event.snapshot.phase) {
            case storage_service::OperationPhase::kStarted:
                overlay_err = overlay_runtime::ShowStorageModalFormatting();
                break;
            case storage_service::OperationPhase::kSucceeded:
                // The format wiped every recording and cached summary; reset both services so the
                // dashboard badges/progress and the Summarize page don't render stale state when
                // the user returns to them.
                recording_archive_service::ResetForFormat();
                summary_service::ResetForFormat();
                overlay_err = overlay_runtime::ShowStorageModalFormatSuccess();
                break;
            case storage_service::OperationPhase::kFailed:
                overlay_err =
                    event.snapshot.last_error == ESP_ERR_NOT_FOUND
                        ? overlay_runtime::ShowStorageModalNoSdCard()
                        : overlay_runtime::ShowStorageModalFormatError();
                break;
            case storage_service::OperationPhase::kIdle:
            default:
                break;
        }
        if (overlay_err != ESP_OK && overlay_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Storage modal update failed: %s", esp_err_to_name(overlay_err));
        }
        FlushOverlayFeedback();
    }

    const bool formatting_in_progress =
        event.snapshot.operation == storage_service::Operation::kFormatSd &&
        event.snapshot.phase == storage_service::OperationPhase::kStarted;
    if (!formatting_in_progress) {
        (void)SyncSettingsPageState(true);
        return;
    }

    ESP_LOGI(kTag, "Storage intent: skipping settings page refresh during active format");
}

void HandleTimezoneEvent(const timezone_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Time intent: enabled=%d timezone=%s valid=%d source=%s ntp_synced=%d syncing=%d date=%s time=%s",
             event.snapshot.settings.enabled ? 1 : 0,
             event.snapshot.settings.timezone_name.empty()
                 ? "<unset>"
                 : event.snapshot.settings.timezone_name.c_str(),
             event.snapshot.runtime.time_valid ? 1 : 0,
             timezone_service::TimeSourceName(event.snapshot.runtime.time_source),
             event.snapshot.runtime.has_network_sync ? 1 : 0,
             event.snapshot.runtime.sync_in_progress ? 1 : 0,
             event.snapshot.runtime.current_date.empty()
                 ? "--"
                 : event.snapshot.runtime.current_date.c_str(),
             event.snapshot.runtime.current_time.empty()
                 ? "--:--"
                 : event.snapshot.runtime.current_time.c_str());

    const esp_err_t lock_screen_err = lock_screen_runtime::SyncClockState(true);
    if (lock_screen_err != ESP_OK && lock_screen_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Lock screen clock update after time event failed: %s",
                 esp_err_to_name(lock_screen_err));
    }

    // Keep the time page in sync on clock events when it is the active screen. If an overlay
    // is open over it, ui_refresh_runtime suppresses the underlay repaint globally (the state
    // is still applied), so we don't need to special-case overlays here.
    const bool time_active = ScreenActiveForRefresh(display_service::ScreenId::kTime);
    const esp_err_t time_page_err = time_page_runtime::SyncFromService(time_active);
    if (time_page_err != ESP_OK && time_page_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Time page update after time event failed: %s",
                 esp_err_to_name(time_page_err));
    }

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after time event failed: %s",
                 esp_err_to_name(status_bar_err));
    }

    // Roll the dashboard welcome message over to the next variant when its interval elapses.
    if (ScreenActiveForRefresh(display_service::ScreenId::kHome)) {
        const esp_err_t welcome_err = dashboard_page_runtime::RefreshWelcomeIfRotated();
        if (welcome_err != ESP_OK && welcome_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Dashboard welcome rotation after time event failed: %s",
                     esp_err_to_name(welcome_err));
        }
    }
}

void RegisterWifiBackendRoutes(httpd_handle_t server, void*)
{
    timezone_service::RegisterPortalRoutes(server);
    gemini_service::RegisterPortalRoutes(server);
}

void HandleGeminiEvent(const gemini_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Gemini intent: configured=%d source=%s ready=%d auth_checked=%d in_flight=%d http=%d status=%s error=%s",
             event.snapshot.settings.configured ? 1 : 0,
             gemini_service::ApiKeySourceName(event.snapshot.settings.api_key_source),
             event.snapshot.runtime.ready ? 1 : 0,
             event.snapshot.runtime.auth_checked ? 1 : 0,
             event.snapshot.runtime.request_in_flight ? 1 : 0,
             event.snapshot.runtime.last_http_status,
             event.snapshot.runtime.last_status_message.empty()
                 ? "<none>"
                 : event.snapshot.runtime.last_status_message.c_str(),
             event.snapshot.runtime.last_error_code.empty()
                 ? "<none>"
                 : event.snapshot.runtime.last_error_code.c_str());

    const bool ready = event.snapshot.runtime.ready;
    const bool was_ready = s_gemini_ready.exchange(ready, std::memory_order_relaxed);
    if (ready && !was_ready) {
        PlayFeedback(feedback_service::FeedbackEvent::kGeminiConnected);
    }

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after Gemini event failed: %s",
                 esp_err_to_name(status_bar_err));
    }
}

void HandleWifiEvent(const wifi_service::Event& event, void*)
{
    ESP_LOGI(kTag,
             "Wi-Fi intent: state=%s detail=%s enabled=%d connected=%d ap=%d ssid=%s ip=%s ap_ssid=%s ap_url=%s rssi=%d",
             wifi_service::StateName(event.state),
             event.detail.empty() ? "" : event.detail.c_str(),
             event.ui_state.wifi_enabled ? 1 : 0,
             event.ui_state.connected ? 1 : 0,
             event.ui_state.access_point_mode ? 1 : 0,
             event.ui_state.ssid.empty() ? "<none>" : event.ui_state.ssid.c_str(),
             event.ui_state.ip_address.empty() ? "<none>" : event.ui_state.ip_address.c_str(),
             event.ui_state.ap_ssid.empty() ? "<none>" : event.ui_state.ap_ssid.c_str(),
             event.ui_state.ap_url.empty() ? "<none>" : event.ui_state.ap_url.c_str(),
             event.ui_state.rssi);
    timezone_service::SetNetworkConnected(event.ui_state.connected);
    gemini_service::SetNetworkState(event.ui_state.connected,
                                    event.ui_state.access_point_mode);

    const esp_err_t status_bar_err =
        s_startup_complete.load(std::memory_order_relaxed)
            ? status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                  display_service::RefreshMode::kPartial)
            : status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Status bar update after Wi-Fi event failed: %s",
                 esp_err_to_name(status_bar_err));
    }

    (void)SyncSettingsPageState(true);
    (void)SyncWifiPageState(true);
}

void HandleDispatchedButtonEvent(const button_service::ButtonEventInfo& event)
{
    ESP_LOGI(kTag, "Button intent: button=%s event=%s pressed_ms=%lu",
             ButtonIdName(event.button), ButtonEventName(event.event),
             static_cast<unsigned long>(event.pressed_ms));

    const app_interaction::InputResult overlay_result =
        input_focus_runtime::HandleButtonEvent(event);
    PlayInteractionFeedback(overlay_result);
    if (overlay_result.select_modal_submitted) {
        if (!notes_page_runtime::HandleItemActionSelection(
                overlay_result.select_modal_selected_index) &&
            !todos_page_runtime::HandleItemActionSelection(
                overlay_result.select_modal_selected_index) &&
            !follow_up_page_runtime::HandleItemActionSelection(
                overlay_result.select_modal_selected_index) &&
            !time_page_runtime::HandleSelectModalSubmit(
                overlay_result.select_modal_selected_index)) {
            (void)recording_session_service::SubmitTagSelection(
                overlay_result.select_modal_selected_index);
        }
        ShowDetailsScreenIfRequested();
    }
    if (overlay_result.request_format_sd_card) {
        const esp_err_t err = storage_service::RequestFormatSdCard();
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Format SD request failed: %s", esp_err_to_name(err));
            const storage_service::Snapshot snapshot = storage_service::GetSnapshot();
            const esp_err_t overlay_err =
                (!snapshot.inserted || err == ESP_ERR_NOT_FOUND)
                    ? overlay_runtime::ShowStorageModalNoSdCard()
                    : overlay_runtime::ShowStorageModalFormatError();
            FlushOverlayFeedback();
            if (overlay_err != ESP_OK && overlay_err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Format SD error modal failed: %s",
                         esp_err_to_name(overlay_err));
            }
        }
    }
    if (overlay_result.request_shutdown && s_shutdown_task != nullptr) {
        xTaskNotifyGive(s_shutdown_task);
    }
    if (overlay_result.consumed) {
        return;
    }

    if (device_sleep_runtime::ConsumeWakeOnlyPowerButtonEvent(event)) {
        return;
    }

    if (storage_service::IsWriteBusy()) {
        ESP_LOGI(kTag, "Button ignored while storage write is active");
        return;
    }

    // No UP+power shutdown chord on this board. That gesture existed for the Sticky's
    // discrete power latch; here the AXP2101 owns power-off through its own key, so a
    // chord would only duplicate it and can misfire during navigation.

    const device_sleep_service::Stage stage_before =
        device_sleep_service::GetSnapshot().runtime.stage;
    device_sleep_runtime::NotifyUserActivity();
    if (event.button == button_service::ButtonId::kAction &&
        stage_before == device_sleep_service::Stage::kDisplaySleeping) {
        device_sleep_runtime::ArmPowerButtonWakeGesture("display-sleep wake");
        return;
    }

    // FN long-press opens the shutdown confirmation. This is the only entry point to
    // the software shutdown path (clean RTC-interrupt clear, then AXP2101 PowerOff);
    // the AXP2101's own power key still hard-cuts the rails without it. Handled here
    // rather than in the switch below because per-screen page input consumes primary
    // long-press without acting on it.
    if (event.button == button_service::ButtonId::kFunction &&
        event.event == button_service::ButtonEvent::kLongPressStart) {
        if constexpr (!kEnablePowerButtonShutdown) {
            ESP_LOGW(kTag, "FN long-press shutdown requested but shutdown is disabled");
            return;
        }
        if (s_shutdown_task == nullptr) {
            ESP_LOGW(kTag, "FN long-press shutdown requested but shutdown task unavailable");
            return;
        }

        ESP_LOGI(kTag, "FN long-press: showing shutdown confirmation");
        const esp_err_t err = overlay_runtime::ShowShutdownModal();
        FlushOverlayFeedback();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Show shutdown modal failed: %s", esp_err_to_name(err));
        }
        return;
    }

    // The ACTION press/hold gesture (arm on press-down, start on long-press,
    // stop/cancel on release) is a global recording control, so it must be
    // evaluated before per-screen page input. Taps (single/double click) fall
    // through the switch's default below and are routed to the page handlers.
    if (event.button == button_service::ButtonId::kAction) {
        const recording_session_service::Context recording_context =
            BuildRecordingSessionContext();
        bool handled = false;
        switch (event.event) {
            case button_service::ButtonEvent::kPressDown:
                handled = recording_session_service::HandlePowerPressDown(recording_context);
                break;
            case button_service::ButtonEvent::kLongPressStart:
                handled = recording_session_service::HandlePowerLongPressStart(recording_context);
                break;
            case button_service::ButtonEvent::kPressUp:
                handled = recording_session_service::HandlePowerPressUp(recording_context);
                break;
            default:
                break;
        }
        if (handled) {
            return;
        }
    }

    const page_input_runtime::ButtonResult page_button_result =
        page_input_runtime::HandleButtonEventForCurrentScreen(event);
    if (page_button_result.handled) {
        PlayInteractionFeedback(page_button_result.interaction_result);
        if (page_button_result.footer_item != footer_runtime::FooterFocusItem::kNone) {
            const app_interaction::InputResult footer_result =
                HandleFooterActivate(page_button_result.footer_item, nullptr);
            PlayInteractionFeedback(footer_result);
        }
        HandleDetailsBackIfRequested();
        HandleOnboardingDismissIfRequested();
        ShowOnboardingFromSettingsIfRequested();
        FlushOverlayFeedback();
        return;
    }

    switch (event.event) {
        case button_service::ButtonEvent::kSingleClick:
            // Intentionally inert. UP/DOWN navigation is driven on press-down/
            // repeat via input_focus_runtime, and POWER_OK activation via page
            // input above. In particular, UP/DOWN must not drive anything while
            // the lock screen is active -- this previously buzzed and forced a
            // lock-screen refresh, making the keys feel live behind the lock.
            break;
        case button_service::ButtonEvent::kDoubleClick:
            // FN double-click toggles the lock screen. DOWN double-click is reserved as
            // the app-wide "exit an entered UI" gesture and is handled by the per-screen page
            // input (WiFi network list, Vibe Check card, ...); it is intentionally a no-op at
            // this level. Any other button just plays the double-click cue.
            if (event.button == button_service::ButtonId::kFunction) {
                const bool was_active = lock_screen_runtime::IsActive();
                const esp_err_t err = lock_screen_runtime::Toggle();
                if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                    ESP_LOGW(kTag, "Power button lock toggle failed: %s",
                             esp_err_to_name(err));
                } else {
                    PlayFeedback(was_active ? feedback_service::FeedbackEvent::kUnlock
                                            : feedback_service::FeedbackEvent::kLock);
                }
            } else if (event.button != button_service::ButtonId::kDown) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonDoubleClick);
            }
            break;
        case button_service::ButtonEvent::kLongPressStart:
            // ACTION's long press starts a recording and plays its own cue.
            if (event.button != button_service::ButtonId::kAction) {
                PlayFeedback(feedback_service::FeedbackEvent::kButtonLongPress);
            }
            break;
        default:
            break;
    }

}

void HandleButtonEvent(const button_service::ButtonEventInfo& event, void*)
{
    button_input_runtime::HandleHardwareEvent(
        event,
        [](const button_service::ButtonEventInfo& dispatched_event) {
            HandleDispatchedButtonEvent(dispatched_event);
        });
}

void ShutdownTask(void*)
{
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGW(kTag, "Shutdown request accepted; waiting for power button release settle");
        PlayFeedback(feedback_service::FeedbackEvent::kShutdown);
        vTaskDelay(kPowerButtonReleaseSettleDelay);
        status_bar_runtime::SetShutdownIndicatorVisible(true);
        const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayStateAndRefreshNow(
            display_service::RefreshMode::kPartial);
        if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Shutdown indicator refresh failed: %s",
                     esp_err_to_name(status_bar_err));
        }
        ESP_LOGW(kTag, "Power button release settled; releasing power hold");
        const esp_err_t err = power_service::RequestShutdown();
        if (err != ESP_OK) {
            overlay_runtime::SetShutdownRequestInProgress(false);
            status_bar_runtime::SetShutdownIndicatorVisible(false);
            const esp_err_t clear_err =
                status_bar_runtime::UpdateDisplayStateAndRequestRefresh(
                    display_service::RefreshMode::kPartial);
            if (clear_err != ESP_OK && clear_err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Shutdown indicator clear failed: %s",
                         esp_err_to_name(clear_err));
            }
            ESP_LOGW(kTag, "Shutdown request failed: %s", esp_err_to_name(err));
        } else {
            overlay_runtime::SetShutdownRequestInProgress(false);
            ESP_LOGW(kTag, "Shutdown request returned; board may still be powered");
        }
    }
}

void StartShutdownTask()
{
    if (s_shutdown_task != nullptr) {
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        ShutdownTask,
        "app_shutdown",
        kShutdownTaskStackWords,
        nullptr,
        followup_task_config::kPriorityAppShutdown,
        &s_shutdown_task,
        followup_task_config::kAppCore);
    if (created != pdPASS) {
        s_shutdown_task = nullptr;
        ESP_LOGW(kTag, "Failed to create shutdown task");
    }
}

void InitButtonService()
{
    const esp_err_t err = input_runtime_setup::InitButtonInput();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Button input init failed: %s", esp_err_to_name(err));
    }
}

void InitFeedbackService()
{
    const esp_err_t err = feedback_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Feedback service init failed: %s", esp_err_to_name(err));
        return;
    }
}

void InitDisplayService()
{
    const esp_err_t err = display_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Display service init failed: %s", esp_err_to_name(err));
    }
}

void InitUiRefreshRuntime()
{
    const esp_err_t err = ui_refresh_runtime::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "UI refresh runtime init failed: %s", esp_err_to_name(err));
    }
}

void InitLockScreenRuntime()
{
    const esp_err_t err = lock_screen_runtime::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Lock screen runtime init failed: %s", esp_err_to_name(err));
    }
}

void InitOverlayRuntime()
{
    const esp_err_t err = overlay_runtime::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Overlay runtime init failed: %s", esp_err_to_name(err));
    }
}

void InitStorageService()
{
    storage_service::SetEventHandler(HandleStorageEvent, nullptr);
    const esp_err_t err = storage_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Storage service init failed: %s", esp_err_to_name(err));
    }
    storage_service::LogDebugStatus();
}

void InitTimezoneService()
{
    timezone_service::SetEventHandler(HandleTimezoneEvent, nullptr);
    const esp_err_t err = timezone_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Timezone service init failed: %s", esp_err_to_name(err));
    }
}

void HandleRecordingArchiveEvent(const recording_archive_service::Event&, void*)
{
    const bool home_active = ScreenActiveForRefresh(display_service::ScreenId::kHome);
    const esp_err_t err = dashboard_page_runtime::SyncFromService(home_active);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Dashboard update after archive event failed: %s", esp_err_to_name(err));
    }

    // The archive changed (recording saved / deleted / re-tagged / follow-up toggled). If one of
    // the archive-backed feature pages is on screen, re-sync it so it does not show stale data.
    // Non-active pages re-sync on entry (ShowXScreen), so only the current screen is refreshed.
    esp_err_t page_err = ESP_OK;
    switch (display_service::GetCurrentScreen()) {
        case display_service::ScreenId::kNotes:
            page_err = notes_page_runtime::SyncFromArchive(true);
            break;
        case display_service::ScreenId::kTodos:
            page_err = todos_page_runtime::SyncFromArchive(true);
            break;
        case display_service::ScreenId::kFollowUp:
            page_err = follow_up_page_runtime::SyncFromArchive(true);
            break;
        case display_service::ScreenId::kVibeCheck:
            page_err = vibe_check_page_runtime::SyncFromService(true);
            break;
        case display_service::ScreenId::kSummarize:
            page_err = summarize_page_runtime::SyncFromService(true);
            break;
        case display_service::ScreenId::kDetails:
            // Re-sync so a just-finished transcription replaces the empty state (and drops the
            // Transcribe button) without leaving the page.
            page_err = details_page_runtime::SyncFromArchive(true);
            break;
        default:
            break;
    }
    if (page_err != ESP_OK && page_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Active page update after archive event failed: %s",
                 esp_err_to_name(page_err));
    }
}

void InitRecordingArchiveService()
{
    recording_archive_service::SetEventHandler(HandleRecordingArchiveEvent, nullptr);
    recording_archive_service::Init();
}

void InitGeminiService()
{
    gemini_service::SetEventHandler(HandleGeminiEvent, nullptr);
    const esp_err_t err = gemini_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Gemini service init failed: %s", esp_err_to_name(err));
    }
}

void InitWifiService()
{
    wifi_service::SetEventHandler(HandleWifiEvent, nullptr);
    wifi_service::SetPortalRouteRegistrar(RegisterWifiBackendRoutes, nullptr);
    const esp_err_t err = wifi_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Wi-Fi service init failed: %s", esp_err_to_name(err));
        return;
    }
    wifi_service::Start();
}

void InitRecordingService()
{
    recording_service::SetEventHandler(HandleRecordingEvent, nullptr);
    const esp_err_t err = recording_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Recording service init failed: %s", esp_err_to_name(err));
        return;
    }

    recording_service::LogDebugStatus();
}

void InitTranscriptionService()
{
    transcription_service::SetEventHandler(HandleTranscriptionEvent, nullptr);
    const esp_err_t err = transcription_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Transcription service init failed: %s", esp_err_to_name(err));
    }
}

void InitRecordingSessionService()
{
    recording_session_service::SetEventHandler(HandleRecordingSessionEvent, nullptr);
    const esp_err_t err = recording_session_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Recording session service init failed: %s", esp_err_to_name(err));
    }
}

void InitFooterRuntime()
{
    footer_runtime::SetActivateHandler(HandleFooterActivate, nullptr);
    dashboard_page_runtime::SetMenuItemHandler(HandleDashboardMenuItem, nullptr);
    footer_runtime::SetLayoutState(FooterLayoutForScreen(display_service::ScreenId::kHome));
    footer_runtime::SetProjectionState(
        page_input_runtime::BuildFooterProjectionForScreen(display_service::ScreenId::kHome));
    const esp_err_t err = footer_runtime::UpdateDisplayState();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer runtime init failed: %s", esp_err_to_name(err));
    }

    const esp_err_t settings_err = settings_page_runtime::UpdateDisplayState();
    if (settings_err != ESP_OK && settings_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Settings page runtime init failed: %s",
                 esp_err_to_name(settings_err));
    }

    const esp_err_t wifi_err = wifi_page_runtime::SyncFromService(false);
    if (wifi_err != ESP_OK && wifi_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "WiFi page runtime init failed: %s", esp_err_to_name(wifi_err));
    }
}

void InitImuService()
{
    const esp_err_t err = imu_service::Init();
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "IMU service init failed: %s", esp_err_to_name(err));
        return;
    }
    imu_service::LogDebugStatus();
}

void InitDeviceSleepRuntime()
{
    device_sleep_runtime::SetShutdownPendingProvider(IsShutdownPending, nullptr);

    device_sleep_runtime::AutoSleepSettings settings = {};
    settings.enabled = true;
    settings.display_sleep_timeout_seconds = kAutoSleepDisplaySleepTimeoutSeconds;
    settings.light_sleep_timeout_seconds = kAutoSleepLightSleepTimeoutSeconds;
    settings.motion_wake_enabled = true;
    settings.interaction_wake_enabled = true;

    const esp_err_t err = device_sleep_runtime::StartAutoSleep(settings);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Device auto-sleep init failed: %s", esp_err_to_name(err));
    }

    const esp_err_t motion_err = device_sleep_runtime::StartMotionPolling();
    if (motion_err != ESP_OK) {
        ESP_LOGW(kTag, "Device sleep motion polling init failed: %s",
                 esp_err_to_name(motion_err));
    }
}

}  // namespace

void Run()
{
    ESP_ERROR_CHECK(power_service::EnablePowerHold());
    ConfirmPendingOtaImage();
    ESP_ERROR_CHECK(power_service::Init());
    power_service::LogDebugStatus();
    InitFeedbackService();
    // On this board the SD card needs to enter and stay in SPI mode before
    // the shared-bus display path is brought up.
    InitStorageService();
    InitDisplayService();
    InitUiRefreshRuntime();
    InitLockScreenRuntime();
    InitOverlayRuntime();
    input_runtime_setup::Configure({
        .inputs_enabled = &InputsEnabled,
        .inputs_enabled_context = nullptr,
        .button_handler = &HandleButtonEvent,
        .button_handler_context = nullptr,
    });
    PlayFeedback(feedback_service::FeedbackEvent::kStartup);
    InitImuService();
    InitDeviceSleepRuntime();
    InitTimezoneService();
    InitRecordingArchiveService();
    InitGeminiService();
    InitWifiService();
    InitRecordingService();
    InitTranscriptionService();
    InitRecordingSessionService();
    InitFooterRuntime();
    StartShutdownTask();
    InitButtonService();
    const esp_err_t status_bar_err = status_bar_runtime::UpdateDisplayState();
    if (status_bar_err != ESP_OK && status_bar_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial status bar update failed: %s", esp_err_to_name(status_bar_err));
    }
    const esp_err_t footer_err = footer_runtime::UpdateDisplayState();
    if (footer_err != ESP_OK && footer_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial footer update failed: %s", esp_err_to_name(footer_err));
    }
    // First-run users see the onboarding carousel; returning users go straight to the dashboard.
    const bool show_onboarding = !OnboardingViewed();
    const esp_err_t initial_err =
        show_onboarding ? ShowOnboardingScreen(display_service::RefreshMode::kFull)
                        : ShowHomeScreen(display_service::RefreshMode::kFull);
    if (initial_err != ESP_OK && initial_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Initial %s show failed: %s", show_onboarding ? "onboarding" : "home screen",
                 esp_err_to_name(initial_err));
    }
    s_startup_complete.store(true, std::memory_order_relaxed);
}

}  // namespace app_shell
