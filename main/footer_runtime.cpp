#include "footer_runtime.h"

#include "esp_check.h"
#include "esp_log.h"
#include "project_assets.h"
#include "recording_session_service.h"
#include "recording_service.h"
#include "ui_refresh_runtime.h"

namespace footer_runtime {
namespace {

constexpr const char* kTag = "FooterRuntime";

std::mutex s_state_mutex;
LayoutState s_layout_state = {};
ProjectionState s_projection_state = {};
int32_t s_interaction_generation = 1;

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

FooterFocusItem FooterFocusItemFromTargetIndex(int32_t index)
{
    switch (static_cast<FooterFocusItem>(index)) {
        case FooterFocusItem::kHome:
        case FooterFocusItem::kSettings:
        case FooterFocusItem::kWifi:
        case FooterFocusItem::kTime:
        case FooterFocusItem::kFolder:
        case FooterFocusItem::kMic:
            return static_cast<FooterFocusItem>(index);
        case FooterFocusItem::kNone:
        default:
            return FooterFocusItem::kNone;
    }
}

FooterFocusItem FooterFocusItemFromUi(epaper_ui::GlobalFooterItemId item)
{
    switch (item) {
        case epaper_ui::GlobalFooterItemId::kHome:
            return FooterFocusItem::kHome;
        case epaper_ui::GlobalFooterItemId::kSettings:
            return FooterFocusItem::kSettings;
        case epaper_ui::GlobalFooterItemId::kWifi:
            return FooterFocusItem::kWifi;
        case epaper_ui::GlobalFooterItemId::kTime:
            return FooterFocusItem::kTime;
        case epaper_ui::GlobalFooterItemId::kFolder:
            return FooterFocusItem::kFolder;
        case epaper_ui::GlobalFooterItemId::kMic:
            return FooterFocusItem::kMic;
        case epaper_ui::GlobalFooterItemId::kNone:
        default:
            return FooterFocusItem::kNone;
    }
}

bool IsItemVisible(const LayoutState& layout, FooterFocusItem item)
{
    if (!layout.visible) {
        return false;
    }

    switch (item) {
        case FooterFocusItem::kHome:
            return layout.show_home;
        case FooterFocusItem::kSettings:
            return layout.show_settings;
        case FooterFocusItem::kWifi:
            return layout.show_wifi;
        case FooterFocusItem::kTime:
            return layout.show_time;
        case FooterFocusItem::kFolder:
            return layout.show_folder;
        case FooterFocusItem::kMic:
            return layout.show_mic;
        case FooterFocusItem::kNone:
        default:
            return false;
    }
}

bool LayoutStateEquals(const LayoutState& lhs, const LayoutState& rhs)
{
    return lhs.visible == rhs.visible && lhs.show_home == rhs.show_home &&
           lhs.show_settings == rhs.show_settings && lhs.show_wifi == rhs.show_wifi &&
           lhs.show_time == rhs.show_time && lhs.show_folder == rhs.show_folder &&
           lhs.show_mic == rhs.show_mic;
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

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }

    const epaper_ui::GlobalFooterState state = BuildState();
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        generation = s_interaction_generation;
    }
    epaper_ui::GlobalFooterItemId item = epaper_ui::GlobalFooterItemId::kNone;
    if (!epaper_ui::HitTestGlobalFooterItem(display_service::PortraitWidth(),
                                            display_service::PortraitHeight(),
                                            state,
                                            x,
                                            y,
                                            &item)) {
        return false;
    }

    const FooterFocusItem focused_item = FooterFocusItemFromUi(item);
    if (focused_item == FooterFocusItem::kNone) {
        return false;
    }

    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kFooter,
            .kind = app_interaction::Kind::kFooterItem,
            .primary_index = static_cast<int32_t>(focused_item),
            .secondary_index = generation,
        };
    }
    return true;
}

bool FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    if (target.owner != app_interaction::Owner::kFooter ||
        target.kind != app_interaction::Kind::kFooterItem) {
        return false;
    }

    const FooterFocusItem item = FooterFocusItemFromTargetIndex(target.primary_index);
    if (item == FooterFocusItem::kNone) {
        return false;
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale footer focus target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return false;
        }
        if (!IsItemVisible(s_layout_state, item)) {
            return false;
        }
        if (s_projection_state.focused_item != item) {
            s_projection_state.focused_item = item;
            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    const esp_err_t err = UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Footer refresh after touch focus failed: %s", esp_err_to_name(err));
    }
    return true;
}

app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target)
{
    app_interaction::InputResult result = {};
    if (target.owner != app_interaction::Owner::kFooter ||
        target.kind != app_interaction::Kind::kFooterItem) {
        return result;
    }

    const FooterFocusItem item = FooterFocusItemFromTargetIndex(target.primary_index);
    if (item == FooterFocusItem::kNone) {
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (target.secondary_index != s_interaction_generation) {
            ESP_LOGW(kTag,
                     "Ignoring stale footer activate target: primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_interaction_generation));
            return result;
        }
        if (!IsItemVisible(s_layout_state, item)) {
            return result;
        }
    }

    result.consumed = true;
    ESP_LOGI(kTag, "Footer touch activate item=%d (no action bound yet)", static_cast<int>(item));
    return result;
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
