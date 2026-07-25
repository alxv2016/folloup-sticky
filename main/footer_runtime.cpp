#include "footer_runtime.h"

#include "esp_check.h"
#include "esp_log.h"
#include "project_assets.h"
#include "recording_service.h"
#include "ui_refresh_runtime.h"

namespace footer_runtime {
namespace {

constexpr const char* kTag = "FooterRuntime";

std::mutex s_state_mutex;
LayoutState s_layout_state = {};
ProjectionState s_projection_state = {};
int32_t s_interaction_generation = 1;
ActivateHandler s_activate_handler = nullptr;
void* s_activate_context = nullptr;

const EmbeddedImageAsset* FooterIcon(FooterFocusItem item)
{
    switch (item) {
        case FooterFocusItem::kHome:
            return project_assets::GetIcon(EmbeddedIconId::kHome);
        case FooterFocusItem::kSettings:
            return project_assets::GetIcon(EmbeddedIconId::kSettings);
        case FooterFocusItem::kWifi:
            return project_assets::GetIcon(EmbeddedIconId::kWifiConfig);
        case FooterFocusItem::kTime:
            return project_assets::GetIcon(EmbeddedIconId::kTime);
        case FooterFocusItem::kFolder:
            return project_assets::GetIcon(EmbeddedIconId::kFolder);
        case FooterFocusItem::kSticky:
            return project_assets::GetIcon(EmbeddedIconId::kSticky);
        case FooterFocusItem::kMic:
        case FooterFocusItem::kNone:
        default:
            return nullptr;
    }
}

void ApplyProjectedSelection(epaper_ui::GlobalFooterState* state, FooterFocusItem focused_item)
{
    if (state == nullptr) {
        return;
    }

    state->home.selected = focused_item == FooterFocusItem::kHome;
    state->settings.selected = focused_item == FooterFocusItem::kSettings;
    state->wifi.selected = focused_item == FooterFocusItem::kWifi;
    state->time.selected = focused_item == FooterFocusItem::kTime;
    state->folder.selected = focused_item == FooterFocusItem::kFolder;
    state->sticky.selected = focused_item == FooterFocusItem::kSticky;
    state->mic.selected = focused_item == FooterFocusItem::kMic;
}

bool IsMicActive()
{
    // The mic indicator reflects recording (and the brief pre-record preview),
    // NOT the armed state: arming happens on press-down, so keying off it made the
    // mic flash on every quick tap/single-click. The preview lights a little
    // before recording engages so it still feels responsive (see the recorder's
    // kMicPreviewArmDelayUs). Post-capture phases (saving/transcribing) are
    // surfaced by their own overlays/toasts, not the mic.
    if (!recording_service::IsInitialized()) {
        return false;
    }
    const recording_service::UiState recording_state = recording_service::GetUiState();
    return recording_state.preview || recording_state.recording;
}

bool LayoutStateEquals(const LayoutState& lhs, const LayoutState& rhs)
{
    return lhs.visible == rhs.visible && lhs.show_home == rhs.show_home &&
           lhs.show_settings == rhs.show_settings && lhs.show_wifi == rhs.show_wifi &&
           lhs.show_time == rhs.show_time && lhs.show_folder == rhs.show_folder &&
           lhs.show_mic == rhs.show_mic && lhs.show_sticky == rhs.show_sticky;
}

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
        return;
    }
    ++s_interaction_generation;
}

}  // namespace

void SetActivateHandler(ActivateHandler handler, void* context)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_activate_handler = handler;
    s_activate_context = context;
}

void SetLayoutState(const LayoutState& state)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    if (!LayoutStateEquals(s_layout_state, state)) {
        AdvanceInteractionGenerationLocked();
    }
    s_layout_state = state;
}

void SetProjectionState(const ProjectionState& state)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_projection_state = state;
}

LayoutState GetLayoutState()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_layout_state;
}

ProjectionState GetProjectionState()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_projection_state;
}

epaper_ui::GlobalFooterState BuildState()
{
    LayoutState layout = {};
    ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        layout = s_layout_state;
        projection = s_projection_state;
    }

    epaper_ui::GlobalFooterState state = {};
    state.visible = layout.visible;

    state.home.visible = layout.show_home;
    state.home.icon = FooterIcon(FooterFocusItem::kHome);

    state.settings.visible = layout.show_settings;
    state.settings.icon = FooterIcon(FooterFocusItem::kSettings);

    state.wifi.visible = layout.show_wifi;
    state.wifi.icon = FooterIcon(FooterFocusItem::kWifi);

    state.time.visible = layout.show_time;
    state.time.icon = FooterIcon(FooterFocusItem::kTime);

    state.folder.visible = layout.show_folder;
    state.folder.icon = FooterIcon(FooterFocusItem::kFolder);

    state.sticky.visible = layout.show_sticky;
    state.sticky.icon = FooterIcon(FooterFocusItem::kSticky);

    state.mic.visible = layout.show_mic;
    state.mic.idle_icon = project_assets::GetIcon(EmbeddedIconId::kMicOff);
    state.mic.active_icon = project_assets::GetIcon(EmbeddedIconId::kMicOn);
    state.mic.active = IsMicActive();

    ApplyProjectedSelection(&state, projection.focused_item);
    return state;
}

esp_err_t UpdateDisplayState()
{
    return display_service::SetGlobalFooterState(BuildState());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kFooter, &UpdateDisplayState, refresh_mode);
}

esp_err_t UpdateDisplayStateAndRefreshNow(display_service::RefreshMode refresh_mode)
{
    ESP_RETURN_ON_ERROR(UpdateDisplayState(), "FooterRuntime", "set footer state failed");
    return display_service::RefreshCurrentScreen(refresh_mode);
}

}  // namespace footer_runtime
