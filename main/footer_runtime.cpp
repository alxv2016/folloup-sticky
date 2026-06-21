#include "footer_runtime.h"

#include "esp_check.h"
#include "project_assets.h"
#include "recording_session_service.h"
#include "recording_service.h"
#include "ui_refresh_runtime.h"

namespace footer_runtime {
namespace {

std::mutex s_state_mutex;
LayoutState s_layout_state = {};
ProjectionState s_projection_state = {};

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
    state->mic.selected = focused_item == FooterFocusItem::kMic;
}

bool IsMicActive()
{
    bool active = false;
    if (recording_service::IsInitialized()) {
        const recording_service::UiState recording_state = recording_service::GetUiState();
        active = recording_state.armed || recording_state.recording;
    }

    const recording_session_service::Snapshot session_snapshot =
        recording_session_service::GetSnapshot();
    if (session_snapshot.initialized &&
        (session_snapshot.phase == recording_session_service::Phase::kSaving ||
         session_snapshot.phase == recording_session_service::Phase::kTranscribing)) {
        active = true;
    }

    return active;
}

}  // namespace

void SetLayoutState(const LayoutState& state)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
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
