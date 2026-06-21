#include "overlay_runtime.h"

#include <cstdint>
#include <mutex>

#include "design_tokens.h"
#include "display_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "feedback_service.h"
#include "page_navigation/roving_focus.h"
#include "ui_refresh_runtime.h"

namespace overlay_runtime {
namespace {

constexpr const char* kTag = "OverlayRuntime";

std::mutex s_state_mutex;
bool s_initialized = false;
epaper_ui::ShutdownModalState s_shutdown_modal_state = {};
epaper_ui::SelectModalState s_select_modal_state = {};
epaper_ui::ToastState s_toast_state = {};
page_navigation::RovingFocus s_shutdown_modal_focus = {};
page_navigation::RovingFocus s_select_modal_focus = {};
bool s_shutdown_request_in_progress = false;
esp_timer_handle_t s_toast_timer = nullptr;
uint32_t s_toast_generation = 0;
int32_t s_overlay_interaction_generation = 1;

int NormalizeSelectModalIndex(int selected_index, int item_count)
{
    return item_count > 0 ? page_navigation::RovingFocus::WrapIndex(selected_index, item_count) : 0;
}

int ShutdownActionIndex(epaper_ui::ShutdownModalAction action)
{
    return action == epaper_ui::ShutdownModalAction::kConfirm ? 1 : 0;
}

epaper_ui::ShutdownModalAction ShutdownActionForIndex(int index)
{
    return index == 1 ? epaper_ui::ShutdownModalAction::kConfirm
                      : epaper_ui::ShutdownModalAction::kCancel;
}

void AdvanceOverlayInteractionGenerationLocked()
{
    if (s_overlay_interaction_generation == INT32_MAX) {
        s_overlay_interaction_generation = 1;
        return;
    }
    ++s_overlay_interaction_generation;
}

app_interaction::InteractiveTarget MakeSelectModalItemTarget(int index, int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlaySelectModalItem,
        .primary_index = index,
        .secondary_index = generation,
    };
}

app_interaction::InteractiveTarget MakeShutdownActionTarget(epaper_ui::ShutdownModalAction action,
                                                            int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlayShutdownAction,
        .primary_index = ShutdownActionIndex(action),
        .secondary_index = generation,
    };
}

app_interaction::InteractiveTarget MakeToastCloseTarget(int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlayToastCloseAction,
        .primary_index = 0,
        .secondary_index = generation,
    };
}

void SyncShutdownModalStateFromFocusLocked()
{
    s_shutdown_modal_state.selected_action =
        ShutdownActionForIndex(s_shutdown_modal_focus.index());
}

void SyncSelectModalStateFromFocusLocked()
{
    s_select_modal_state.selected_index = s_select_modal_focus.index();
}

int CurrentSelectModalIndexLocked()
{
    return s_select_modal_focus.index() >= 0 ? s_select_modal_focus.index()
                                             : s_select_modal_state.selected_index;
}

struct OverlayRefreshSnapshot {
    bool shutdown_visible = false;
    bool select_visible = false;
    bool toast_visible = false;
    epaper_ui::UiRect bounds = {};
};

epaper_ui::UiRect ShadowedRect(epaper_ui::UiRect rect, int shadow_offset)
{
    if (rect.IsEmpty()) {
        return {};
    }

    rect.width += shadow_offset;
    rect.height += shadow_offset;
    return rect;
}

epaper_ui::UiRect UnionRect(epaper_ui::UiRect lhs, epaper_ui::UiRect rhs)
{
    if (lhs.IsEmpty()) {
        return rhs;
    }
    if (rhs.IsEmpty()) {
        return lhs;
    }

    const int left = std::min(lhs.x, rhs.x);
    const int top = std::min(lhs.y, rhs.y);
    const int right = std::max(lhs.right(), rhs.right());
    const int bottom = std::max(lhs.bottom(), rhs.bottom());
    return {left, top, right - left, bottom - top};
}

bool RectEquals(const epaper_ui::UiRect& lhs, const epaper_ui::UiRect& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
}

OverlayRefreshSnapshot CaptureOverlayRefreshSnapshotLocked()
{
    OverlayRefreshSnapshot snapshot = {};
    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();

    snapshot.shutdown_visible = s_shutdown_modal_state.visible;
    snapshot.select_visible = s_select_modal_state.visible;
    snapshot.toast_visible = s_toast_state.visible;

    if (snapshot.toast_visible) {
        snapshot.bounds = UnionRect(
            snapshot.bounds,
            ShadowedRect(epaper_ui::ToastPanelBounds(portrait_width, portrait_height, s_toast_state),
                         design::toast::kShadowOffset));
    }
    if (snapshot.select_visible) {
        snapshot.bounds = UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::SelectModalPanelBounds(
                    portrait_width, portrait_height, s_select_modal_state),
                design::modal::kShadowOffset));
    }
    if (snapshot.shutdown_visible) {
        snapshot.bounds = UnionRect(
            snapshot.bounds,
            ShadowedRect(epaper_ui::ShutdownModalCardBounds(portrait_width, portrait_height),
                         design::modal::kShadowOffset));
    }

    return snapshot;
}

display_service::OverlayRefreshPolicy DetermineOverlayRefreshPolicy(
    const OverlayRefreshSnapshot& before, const OverlayRefreshSnapshot& after)
{
    const bool visibility_changed = before.shutdown_visible != after.shutdown_visible ||
                                    before.select_visible != after.select_visible ||
                                    before.toast_visible != after.toast_visible;
    if (visibility_changed || !RectEquals(before.bounds, after.bounds)) {
        return display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    }

    return display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;
}

bool HitShutdownActionAnyOrientation(int portrait_width,
                                     int portrait_height,
                                     int x,
                                     int y,
                                     epaper_ui::ShutdownModalAction* action)
{
    bool hit = false;
    epaper_ui::ShutdownModalAction candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, x, y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    const int mirrored_x = std::max(0, portrait_width - 1 - x);
    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, mirrored_x, y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    const int mirrored_y = std::max(0, portrait_height - 1 - y);
    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, x, mirrored_y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    candidate = epaper_ui::HitTestShutdownModalAction(
        portrait_width, portrait_height, mirrored_x, mirrored_y, &hit);
    if (hit) {
        if (action != nullptr) {
            *action = candidate;
        }
        return true;
    }

    return false;
}

bool HitSelectModalItemAnyOrientation(int portrait_width,
                                      int portrait_height,
                                      const epaper_ui::SelectModalState& state,
                                      int x,
                                      int y,
                                      int* selected_index)
{
    bool hit = false;
    int candidate =
        epaper_ui::HitTestSelectModalItem(portrait_width, portrait_height, state, x, y, &hit);
    if (hit) {
        if (selected_index != nullptr) {
            *selected_index = candidate;
        }
        return true;
    }

    const int mirrored_x = std::max(0, portrait_width - 1 - x);
    candidate = epaper_ui::HitTestSelectModalItem(
        portrait_width, portrait_height, state, mirrored_x, y, &hit);
    if (hit) {
        if (selected_index != nullptr) {
            *selected_index = candidate;
        }
        return true;
    }

    const int mirrored_y = std::max(0, portrait_height - 1 - y);
    candidate = epaper_ui::HitTestSelectModalItem(
        portrait_width, portrait_height, state, x, mirrored_y, &hit);
    if (hit) {
        if (selected_index != nullptr) {
            *selected_index = candidate;
        }
        return true;
    }

    candidate = epaper_ui::HitTestSelectModalItem(
        portrait_width, portrait_height, state, mirrored_x, mirrored_y, &hit);
    if (hit) {
        if (selected_index != nullptr) {
            *selected_index = candidate;
        }
        return true;
    }

    return false;
}

bool IsOverlayTarget(const app_interaction::InteractiveTarget& target)
{
    return target.owner == app_interaction::Owner::kOverlay && target.IsValid();
}

bool IsCurrentOverlayTargetLocked(const app_interaction::InteractiveTarget& target)
{
    return IsOverlayTarget(target) && target.secondary_index == s_overlay_interaction_generation;
}

esp_err_t ApplyOverlayState(const epaper_ui::ShutdownModalState& shutdown_modal_state,
                            const epaper_ui::SelectModalState& select_modal_state,
                            const epaper_ui::ToastState& toast_state)
{
    ESP_RETURN_ON_ERROR(display_service::SetShutdownModalState(shutdown_modal_state),
                        kTag,
                        "set shutdown modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetSelectModalState(select_modal_state),
                        kTag,
                        "set select modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetToastState(toast_state),
                        kTag,
                        "set toast state failed");
    return ESP_OK;
}

esp_err_t SyncOverlayState(
    bool request_refresh,
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay)
{
    if (request_refresh) {
        return ui_refresh_runtime::ScheduleOverlay(ui_refresh_runtime::SurfaceKey::kOverlay,
                                                   []() { return SyncOverlayState(false); },
                                                   refresh_policy);
    }

    epaper_ui::ShutdownModalState shutdown_modal_state = {};
    epaper_ui::SelectModalState select_modal_state = {};
    epaper_ui::ToastState toast_state = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        shutdown_modal_state = s_shutdown_modal_state;
        select_modal_state = s_select_modal_state;
        toast_state = s_toast_state;
    }

    return ApplyOverlayState(shutdown_modal_state, select_modal_state, toast_state);
}

void ToastTimerCallback(void*)
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || !s_toast_state.visible) {
            return;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        s_toast_state = {};
        ++s_toast_generation;
        AdvanceOverlayInteractionGenerationLocked();
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
        changed = true;
    }

    if (!changed) {
        return;
    }

    const esp_err_t err = SyncOverlayState(true, refresh_policy);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Toast timer refresh failed: %s", esp_err_to_name(err));
    }
}

}  // namespace

esp_err_t Init()
{
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (s_initialized) {
            return ESP_OK;
        }
        s_shutdown_modal_state = {};
        s_select_modal_state = {};
        s_toast_state = {};
        s_overlay_interaction_generation = 1;
        s_shutdown_modal_focus.Configure(0);
        s_select_modal_focus.Configure(0);
        s_shutdown_request_in_progress = false;
        if (s_toast_timer == nullptr) {
            esp_timer_create_args_t timer_args = {};
            timer_args.callback = &ToastTimerCallback;
            timer_args.dispatch_method = ESP_TIMER_TASK;
            timer_args.name = "overlay_toast";
            timer_args.skip_unhandled_events = true;
            ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_toast_timer),
                                kTag,
                                "toast timer create failed");
        }
        s_initialized = true;
    }

    ESP_RETURN_ON_ERROR(SyncOverlayState(false), kTag, "initial overlay sync failed");
    ESP_LOGI(kTag, "Overlay runtime initialized");
    return ESP_OK;
}

bool IsShutdownModalVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible;
}

bool IsShutdownPending()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress ||
           s_select_modal_state.visible;
}

bool IsInputCaptured()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress ||
           s_select_modal_state.visible ||
           (s_toast_state.visible && s_toast_state.show_close_button);
}

bool IsSelectModalVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_select_modal_state.visible;
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }

    epaper_ui::SelectModalState select_modal_state = {};
    epaper_ui::ToastState toast_state = {};
    bool shutdown_visible = false;
    bool select_visible = false;
    bool toast_visible = false;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return false;
        }
        shutdown_visible = s_shutdown_modal_state.visible;
        select_visible = s_select_modal_state.visible && !s_shutdown_modal_state.visible;
        toast_visible = s_toast_state.visible;
        select_modal_state = s_select_modal_state;
        toast_state = s_toast_state;
        generation = s_overlay_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();

    if (select_visible) {
        int selected_index = -1;
        if (HitSelectModalItemAnyOrientation(
                portrait_width, portrait_height, select_modal_state, x, y, &selected_index)) {
            if (target != nullptr) {
                *target = MakeSelectModalItemTarget(selected_index, generation);
            }
            return true;
        }
        return false;
    }

    if (shutdown_visible) {
        epaper_ui::ShutdownModalAction action = epaper_ui::ShutdownModalAction::kCancel;
        if (HitShutdownActionAnyOrientation(portrait_width, portrait_height, x, y, &action)) {
            if (target != nullptr) {
                *target = MakeShutdownActionTarget(action, generation);
            }
            return true;
        }
        return false;
    }

    if (toast_visible && toast_state.show_close_button &&
        epaper_ui::HitTestToastCloseButton(portrait_width, portrait_height, toast_state, x, y)) {
        if (target != nullptr) {
            *target = MakeToastCloseTarget(generation);
        }
        return true;
    }

    return false;
}

bool FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    if (!IsOverlayTarget(target)) {
        return false;
    }

    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return false;
        }
        if (!IsCurrentOverlayTargetLocked(target)) {
            ESP_LOGW(kTag,
                     "Ignoring stale overlay focus target: kind=%d primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<int>(target.kind),
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_overlay_interaction_generation));
            return false;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        switch (target.kind) {
            case app_interaction::Kind::kOverlaySelectModalItem:
                if (!s_select_modal_state.visible || s_shutdown_modal_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >= static_cast<int>(s_select_modal_state.items.size())) {
                    return false;
                }
                if (CurrentSelectModalIndexLocked() != target.primary_index) {
                    s_select_modal_focus.SetIndex(target.primary_index);
                    SyncSelectModalStateFromFocusLocked();
                    changed = true;
                }
                break;
            case app_interaction::Kind::kOverlayShutdownAction: {
                if (!s_shutdown_modal_state.visible ||
                    (target.primary_index != 0 && target.primary_index != 1)) {
                    return false;
                }
                const epaper_ui::ShutdownModalAction action =
                    ShutdownActionForIndex(target.primary_index);
                if (s_shutdown_modal_state.selected_action != action) {
                    s_shutdown_modal_focus.SetIndex(target.primary_index);
                    SyncShutdownModalStateFromFocusLocked();
                    changed = true;
                }
                break;
            }
            case app_interaction::Kind::kOverlayToastCloseAction:
                if (!s_toast_state.visible || !s_toast_state.show_close_button ||
                    target.primary_index != 0 ||
                    s_toast_state.close_button_focused) {
                    return false;
                }
                s_toast_state.close_button_focused = true;
                changed = true;
                break;
            case app_interaction::Kind::kNone:
            case app_interaction::Kind::kFooterItem:
            case app_interaction::Kind::kPageAction:
            case app_interaction::Kind::kPageComposite:
            case app_interaction::Kind::kPageListRow:
            default:
                return false;
        }

        if (changed) {
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
        }
    }

    if (!changed) {
        return false;
    }

    const esp_err_t err = SyncOverlayState(true, refresh_policy);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Overlay refresh after touch focus failed: %s", esp_err_to_name(err));
    }
    return true;
}

app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target)
{
    app_interaction::InputResult result = {};
    if (!IsOverlayTarget(target)) {
        return result;
    }

    bool request_refresh = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return result;
        }
        if (!IsCurrentOverlayTargetLocked(target)) {
            ESP_LOGW(kTag,
                     "Ignoring stale overlay activate target: kind=%d primary=%ld target_gen=%ld current_gen=%ld",
                     static_cast<int>(target.kind),
                     static_cast<long>(target.primary_index),
                     static_cast<long>(target.secondary_index),
                     static_cast<long>(s_overlay_interaction_generation));
            return result;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        switch (target.kind) {
            case app_interaction::Kind::kOverlaySelectModalItem:
                if (!s_select_modal_state.visible || s_shutdown_modal_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >= static_cast<int>(s_select_modal_state.items.size())) {
                    return result;
                }
                s_select_modal_focus.SetIndex(target.primary_index);
                SyncSelectModalStateFromFocusLocked();
                result.consumed = true;
                result.select_modal_submitted = true;
                result.select_modal_selected_index = CurrentSelectModalIndexLocked();
                s_select_modal_state = {};
                s_select_modal_focus.Configure(0);
                AdvanceOverlayInteractionGenerationLocked();
                request_refresh = true;
                break;
            case app_interaction::Kind::kOverlayShutdownAction: {
                if (!s_shutdown_modal_state.visible ||
                    (target.primary_index != 0 && target.primary_index != 1)) {
                    return result;
                }
                const epaper_ui::ShutdownModalAction action =
                    ShutdownActionForIndex(target.primary_index);
                s_shutdown_modal_focus.SetIndex(target.primary_index);
                SyncShutdownModalStateFromFocusLocked();
                s_shutdown_modal_state.selected_action = action;
                s_shutdown_modal_state.visible = false;
                s_shutdown_modal_focus.Configure(0);
                AdvanceOverlayInteractionGenerationLocked();
                result.consumed = true;
                if (action == epaper_ui::ShutdownModalAction::kConfirm) {
                    s_shutdown_request_in_progress = true;
                    result.request_shutdown = true;
                }
                request_refresh = true;
                break;
            }
            case app_interaction::Kind::kOverlayToastCloseAction:
                if (!s_toast_state.visible || !s_toast_state.show_close_button ||
                    target.primary_index != 0) {
                    return result;
                }
                if (s_toast_timer != nullptr) {
                    esp_timer_stop(s_toast_timer);
                }
                s_toast_state = {};
                ++s_toast_generation;
                AdvanceOverlayInteractionGenerationLocked();
                result.consumed = true;
                request_refresh = true;
                break;
            case app_interaction::Kind::kNone:
            case app_interaction::Kind::kFooterItem:
            case app_interaction::Kind::kPageAction:
            case app_interaction::Kind::kPageComposite:
            case app_interaction::Kind::kPageListRow:
            default:
                return result;
        }

        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
    }

    if (result.select_modal_submitted) {
        ESP_LOGI(kTag, "Select modal touch activate index=%d", result.select_modal_selected_index);
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayShutdownAction) {
        ESP_LOGI(kTag,
                 "Shutdown modal touch activate action=%s",
                 result.request_shutdown ? "confirm" : "cancel");
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayToastCloseAction) {
        ESP_LOGI(kTag, "Toast close activated by touch");
    }

    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true, refresh_policy);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after touch activate failed: %s",
                     esp_err_to_name(err));
        }
    }
    return result;
}

esp_err_t ShowShutdownModal()
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (!s_shutdown_modal_state.visible ||
            s_shutdown_modal_state.selected_action != epaper_ui::ShutdownModalAction::kCancel) {
            s_shutdown_modal_state.visible = true;
            s_shutdown_modal_focus.Configure(
                2, ShutdownActionIndex(epaper_ui::ShutdownModalAction::kCancel));
            SyncShutdownModalStateFromFocusLocked();
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Shutdown modal shown");
    (void)feedback_service::Play(feedback_service::FeedbackEvent::kModalOpen);
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t DismissShutdownModal()
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_shutdown_modal_state.visible) {
            s_shutdown_modal_state.visible = false;
            s_shutdown_modal_state.selected_action = epaper_ui::ShutdownModalAction::kCancel;
            s_shutdown_modal_focus.Configure(0);
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Shutdown modal dismissed");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowSelectModal(const epaper_ui::SelectModalState& state)
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        s_select_modal_state = state;
        s_select_modal_state.visible = true;
        const int item_count = static_cast<int>(s_select_modal_state.items.size());
        s_select_modal_focus.Configure(s_select_modal_state.items.size(),
                                       NormalizeSelectModalIndex(s_select_modal_state.selected_index,
                                                                 item_count));
        SyncSelectModalStateFromFocusLocked();
        AdvanceOverlayInteractionGenerationLocked();
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
        changed = true;
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Select modal shown");
    (void)feedback_service::Play(feedback_service::FeedbackEvent::kModalOpen);
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t DismissSelectModal()
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_select_modal_state.visible) {
            s_select_modal_state = {};
            s_select_modal_focus.Configure(0);
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Select modal dismissed");
    return SyncOverlayState(true, refresh_policy);
}

bool MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }

    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return false;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_select_modal_state.visible && !s_shutdown_modal_state.visible) {
            if (s_select_modal_focus.item_count() <= 0) {
                return false;
            }

            if (s_select_modal_focus.Move(delta > 0 ? 1 : -1)) {
                SyncSelectModalStateFromFocusLocked();
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        } else if (s_shutdown_modal_state.visible) {
            if (s_shutdown_modal_focus.Move(delta > 0 ? 1 : -1)) {
                SyncShutdownModalStateFromFocusLocked();
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        }
    }

    if (!changed) {
        return false;
    }

    const esp_err_t err = SyncOverlayState(true, refresh_policy);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(kTag, "Overlay refresh after focus move failed: %s", esp_err_to_name(err));
    }
    return true;
}

void SetShutdownRequestInProgress(bool in_progress)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    if (s_shutdown_request_in_progress != in_progress) {
        s_shutdown_request_in_progress = in_progress;
        AdvanceOverlayInteractionGenerationLocked();
    }
}

app_interaction::InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event)
{
    app_interaction::InputResult result = {};
    bool request_refresh = false;
    bool play_click = false;
    bool dismiss_modal = false;
    bool dismiss_select_modal = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return result;
        }
        if (s_shutdown_request_in_progress) {
            result.consumed = true;
            return result;
        }
        if (!s_shutdown_modal_state.visible && !s_select_modal_state.visible) {
            return result;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_select_modal_state.visible && !s_shutdown_modal_state.visible) {
            result.consumed = true;
            switch (event.event) {
                case button_service::ButtonEvent::kSingleClick:
                    if (event.button == button_service::ButtonId::kPowerOk) {
                        result.select_modal_submitted = true;
                        result.select_modal_selected_index = CurrentSelectModalIndexLocked();
                        s_select_modal_state = {};
                        s_select_modal_focus.Configure(0);
                        AdvanceOverlayInteractionGenerationLocked();
                        request_refresh = true;
                        play_click = true;
                        dismiss_select_modal = true;
                        refresh_policy = DetermineOverlayRefreshPolicy(
                            before, CaptureOverlayRefreshSnapshotLocked());
                    }
                    break;
                case button_service::ButtonEvent::kDoubleClick:
                    if (event.button == button_service::ButtonId::kDown &&
                        s_select_modal_focus.item_count() > 0) {
                        result.select_modal_submitted = true;
                        result.select_modal_selected_index = CurrentSelectModalIndexLocked();
                        s_select_modal_state = {};
                        s_select_modal_focus.Configure(0);
                        AdvanceOverlayInteractionGenerationLocked();
                        request_refresh = true;
                        play_click = true;
                        dismiss_select_modal = true;
                        refresh_policy = DetermineOverlayRefreshPolicy(
                            before, CaptureOverlayRefreshSnapshotLocked());
                    }
                    break;
                default:
                    break;
            }
            goto done_locked;
        }

        result.consumed = true;
        switch (event.event) {
            case button_service::ButtonEvent::kSingleClick:
                break;
            case button_service::ButtonEvent::kDoubleClick:
                if (event.button == button_service::ButtonId::kDown) {
                    if (s_shutdown_modal_state.selected_action ==
                        epaper_ui::ShutdownModalAction::kConfirm) {
                        s_shutdown_request_in_progress = true;
                        result.request_shutdown = true;
                    } else {
                        play_click = true;
                    }
                    s_shutdown_modal_state.visible = false;
                    s_shutdown_modal_state.selected_action =
                        epaper_ui::ShutdownModalAction::kCancel;
                    s_shutdown_modal_focus.Configure(0);
                    AdvanceOverlayInteractionGenerationLocked();
                    request_refresh = true;
                    dismiss_modal = true;
                    refresh_policy = DetermineOverlayRefreshPolicy(
                        before, CaptureOverlayRefreshSnapshotLocked());
                }
                break;
            case button_service::ButtonEvent::kPressDown:
            case button_service::ButtonEvent::kPressUp:
            case button_service::ButtonEvent::kLongPressStart:
            case button_service::ButtonEvent::kLongPressUp:
            default:
                break;
        }
    }

done_locked:

    if (play_click) {
        (void)feedback_service::Play(feedback_service::FeedbackEvent::kButtonClick);
    }
    if (dismiss_modal) {
        ESP_LOGI(kTag,
                 "Shutdown modal button action=%s",
                 result.request_shutdown ? "confirm" : "cancel");
    }
    if (dismiss_select_modal) {
        ESP_LOGI(kTag, "Select modal submitted: index=%d", result.select_modal_selected_index);
    }
    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true, refresh_policy);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after button failed: %s", esp_err_to_name(err));
        }
    }
    return result;
}

esp_err_t ShowToast(const epaper_ui::ToastState& state)
{
    bool play_modal_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_toast_timer != nullptr) {
            esp_timer_stop(s_toast_timer);
        }
        s_toast_state = state;
        ++s_toast_generation;
        AdvanceOverlayInteractionGenerationLocked();
        play_modal_feedback = s_toast_state.visible;
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
    }
    if (play_modal_feedback) {
        (void)feedback_service::Play(feedback_service::FeedbackEvent::kModalOpen);
    }
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowToastForDuration(const epaper_ui::ToastState& state, uint32_t duration_ms)
{
    bool play_modal_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_toast_timer != nullptr) {
            esp_timer_stop(s_toast_timer);
        }
        s_toast_state = state;
        ++s_toast_generation;
        AdvanceOverlayInteractionGenerationLocked();
        play_modal_feedback = s_toast_state.visible;
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
    }
    if (play_modal_feedback) {
        (void)feedback_service::Play(feedback_service::FeedbackEvent::kModalOpen);
    }

    ESP_RETURN_ON_ERROR(SyncOverlayState(true, refresh_policy), kTag, "timed toast sync failed");
    if (duration_ms > 0 && s_toast_timer != nullptr) {
        return esp_timer_start_once(s_toast_timer, static_cast<uint64_t>(duration_ms) * 1000ULL);
    }
    return ESP_OK;
}

esp_err_t ClearToast()
{
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_toast_timer != nullptr) {
            esp_timer_stop(s_toast_timer);
        }
        s_toast_state = {};
        ++s_toast_generation;
        AdvanceOverlayInteractionGenerationLocked();
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
    }
    return SyncOverlayState(true, refresh_policy);
}

}  // namespace overlay_runtime
