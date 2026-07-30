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
#include "wifi_page_interactions.h"
#include "wifi_page_coordinator.h"
#include "wifi_service.h"

namespace wifi_page_runtime {
namespace {

constexpr const char* kTag = "WifiPageRuntime";

std::mutex s_mutex;
WifiPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
// Last state handed to the display, so an event that changes nothing renders nothing.
epaper_ui::WifiPageState s_last_rendered_state = {};

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
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
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
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
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

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kWifiPageControls,
                                          old_focus_index,
                                          -1,
                                          -1);
    const page_navigation::PageFocusProjection new_projection =
        page_navigation::ProjectPageFocus(s_coordinator.navigation_model(),
                                          page_navigation::NavigationItemSection::kWifiPageControls,
                                          new_focus_index,
                                          -1,
                                          -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::WifiPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

int VisibleNetworkRowCapacityLocked()
{
    return epaper_ui::WifiPageVisibleNetworkRowCapacity(display_service::PortraitWidth(),
                                                        display_service::PortraitHeight(),
                                                        BuildStateLocked());
}

esp_err_t SyncFocusUi(display_service::RefreshMode refresh_mode, bool include_footer)
{
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

    // Only re-render the WiFi page when the keyboard CLOSES (submit/dismiss). The page
    // sits entirely behind the keyboard overlay while typing, and the keyboard's own
    // overlay refresh already shows each typed character in its input preview. Refreshing
    // the whole page (network list + keyboard) on every keystroke was a ~2s full re-render
    // per key for nothing — that was the keyboard's "super slow" typing. The password
    // input state is still kept in sync above, so the page shows the final text on close.
    if (intent == epaper_ui::KeyboardIntent::kSubmit ||
        intent == epaper_ui::KeyboardIntent::kDismiss) {
        (void)overlay_runtime::DismissKeyboard();
        (void)UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
}

esp_err_t ShowPasswordKeyboardImpl()
{
    epaper_ui::KeyboardState keyboard_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        keyboard_state.visible = true;
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

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetWifiPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(display_service::RefreshRequest{
        .refresh_mode = refresh_mode,
    });
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(
        ui_refresh_runtime::SurfaceKey::kWifiPage, &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta, bool page_jump)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::WifiPageState old_state = {};
    epaper_ui::WifiPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        result = wifi_page_interactions::HandleMoveFocus(
            s_coordinator, delta, page_jump, page_jump ? VisibleNetworkRowCapacityLocked() : 0);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
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

wifi_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return wifi_page_interactions::HandlePrimaryActivate(s_coordinator);
}

wifi_page_interactions::SecondaryActivateResult SecondaryActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return wifi_page_interactions::HandleSecondaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role =
        FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    epaper_ui::WifiPageState old_state = {};
    epaper_ui::WifiPageState new_state = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        old_state = BuildStateLocked();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
        new_state = BuildStateLocked();
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);    return result;
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

esp_err_t SyncFromService(bool request_refresh_if_active)
{
    epaper_ui::WifiPageState next = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.RefreshFromService(wifi_service::GetUiState(), wifi_service::GetScanSnapshot());
        next = BuildStateLocked();
    }

    // Only drive the panel when what we would render actually differs.
    //
    // Entering this page starts a scan, and every wifi_service event that follows lands
    // here. The first of them (kScanning) carries no new content at all: ShowWifiScreen
    // already synced this page and the screen transition renders it. Refreshing anyway
    // meant page entry drove the panel twice -- and because the transition publishes the
    // current screen before its own multi-second drive, that extra command paints the Wi-Fi
    // UI, so it showed up as a stray refresh wrapped around the real one. No other page
    // does this because no other page's Show*Screen starts a service that emits events.
    const bool changed = !(next == s_last_rendered_state);
    s_last_rendered_state = next;
    if (!changed) {
        return ESP_OK;
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

esp_err_t ShowPasswordKeyboard()
{
    return ShowPasswordKeyboardImpl();
}

}  // namespace wifi_page_runtime
