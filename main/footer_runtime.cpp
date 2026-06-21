#include "footer_runtime.h"

#include "esp_check.h"
#include "project_assets.h"
#include "recording_service.h"

namespace footer_runtime {
namespace {

std::mutex s_state_mutex;
epaper_ui::GlobalFooterState s_base_state = {
    .visible = true,
};

}  // namespace

void SetBaseState(const epaper_ui::GlobalFooterState& state)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    s_base_state = state;
}

epaper_ui::GlobalFooterState BuildState()
{
    epaper_ui::GlobalFooterState state = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        state = s_base_state;
    }

    state.visible = true;
    state.mic.visible = true;
    state.mic.idle_icon = project_assets::GetIcon(EmbeddedIconId::kMicOff);
    state.mic.active_icon = project_assets::GetIcon(EmbeddedIconId::kMicOn);

    if (recording_service::IsInitialized()) {
        const recording_service::UiState recording_state = recording_service::GetUiState();
        state.mic.active = recording_state.armed || recording_state.recording;
    } else {
        state.mic.active = false;
    }

    return state;
}

esp_err_t UpdateDisplayState()
{
    return display_service::SetGlobalFooterState(BuildState());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    ESP_RETURN_ON_ERROR(UpdateDisplayState(), "FooterRuntime", "set footer state failed");
    return display_service::RequestRefreshCurrentScreen(refresh_mode);
}

esp_err_t UpdateDisplayStateAndRefreshNow(display_service::RefreshMode refresh_mode)
{
    ESP_RETURN_ON_ERROR(UpdateDisplayState(), "FooterRuntime", "set footer state failed");
    return display_service::RefreshCurrentScreen(refresh_mode);
}

}  // namespace footer_runtime
