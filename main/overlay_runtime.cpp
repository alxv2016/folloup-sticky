#include "overlay_runtime.h"

#include <algorithm>
#include <cstdint>
#include <mutex>

#include "design_tokens.h"
#include "display_service.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "page_navigation/roving_focus.h"
#include "ui_refresh_runtime.h"

namespace overlay_runtime {
namespace {

constexpr const char* kTag = "OverlayRuntime";

std::mutex s_state_mutex;
bool s_initialized = false;
epaper_ui::ShutdownModalState s_shutdown_modal_state = {};
epaper_ui::StorageModalState s_storage_modal_state = {};
epaper_ui::SelectModalState s_select_modal_state = {};
epaper_ui::KeyboardState s_keyboard_state = {};
epaper_ui::ToastState s_toast_state = {};
page_navigation::RovingFocus s_shutdown_modal_focus = {};
page_navigation::RovingFocus s_storage_modal_focus = {};
page_navigation::RovingFocus s_select_modal_focus = {};
bool s_shutdown_request_in_progress = false;
KeyboardEventHandler s_keyboard_event_handler = nullptr;
void* s_keyboard_event_context = nullptr;
esp_timer_handle_t s_toast_timer = nullptr;
uint32_t s_toast_generation = 0;
int32_t s_overlay_interaction_generation = 1;
bool s_feedback_pending = false;
app_interaction::FeedbackCue s_pending_feedback = app_interaction::FeedbackCue::kModalOpen;

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

void QueuePendingFeedbackLocked(app_interaction::FeedbackCue cue)
{
    s_pending_feedback = cue;
    s_feedback_pending = true;
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

app_interaction::InteractiveTarget MakeStorageModalActionTarget(int index, int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlayStorageModalAction,
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

app_interaction::InteractiveTarget MakeKeyboardKeyTarget(int index, int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlayKeyboardKey,
        .primary_index = index,
        .secondary_index = generation,
    };
}

void SyncShutdownModalStateFromFocusLocked()
{
    s_shutdown_modal_state.selected_action =
        ShutdownActionForIndex(s_shutdown_modal_focus.index());
}

int StorageModalActionCountLocked()
{
    switch (s_storage_modal_state.kind) {
        case epaper_ui::StorageModalKind::kConfirmFormat:
            return 2;
        case epaper_ui::StorageModalKind::kNoSdCard:
        case epaper_ui::StorageModalKind::kFormatSuccess:
        case epaper_ui::StorageModalKind::kFormatError:
            return 1;
        case epaper_ui::StorageModalKind::kFormatting:
        case epaper_ui::StorageModalKind::kNone:
        default:
            return 0;
    }
}

void SyncStorageModalStateFromFocusLocked()
{
    s_storage_modal_state.selected_action_index = std::max(0, s_storage_modal_focus.index());
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
    bool storage_visible = false;
    bool select_visible = false;
    bool keyboard_visible = false;
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
    snapshot.storage_visible = s_storage_modal_state.visible;
    snapshot.select_visible = s_select_modal_state.visible;
    snapshot.keyboard_visible = s_keyboard_state.visible;
    snapshot.toast_visible = s_toast_state.visible;

    if (snapshot.toast_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(epaper_ui::ToastPanelBounds(portrait_width, portrait_height, s_toast_state),
                         design::toast::kShadowOffset));
    }
    if (snapshot.keyboard_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::KeyboardPanelBounds(portrait_width, portrait_height, s_keyboard_state, {}),
                design::modal::kShadowOffset));
    }
    if (snapshot.storage_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::StorageModalPanelBounds(
                    portrait_width, portrait_height, s_storage_modal_state),
                design::modal::kShadowOffset));
    }
    if (snapshot.select_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::SelectModalPanelBounds(
                    portrait_width, portrait_height, s_select_modal_state),
                design::modal::kShadowOffset));
    }
    if (snapshot.shutdown_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
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
                                    before.storage_visible != after.storage_visible ||
                                    before.select_visible != after.select_visible ||
                                    before.keyboard_visible != after.keyboard_visible ||
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
                            const epaper_ui::StorageModalState& storage_modal_state,
                            const epaper_ui::SelectModalState& select_modal_state,
                            const epaper_ui::KeyboardState& keyboard_state,
                            const epaper_ui::ToastState& toast_state)
{
    ESP_RETURN_ON_ERROR(display_service::SetShutdownModalState(shutdown_modal_state),
                        kTag,
                        "set shutdown modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetStorageModalState(storage_modal_state),
                        kTag,
                        "set storage modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetSelectModalState(select_modal_state),
                        kTag,
                        "set select modal state failed");
    ESP_RETURN_ON_ERROR(display_service::SetKeyboardState(keyboard_state),
                        kTag,
                        "set keyboard state failed");
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
    epaper_ui::StorageModalState storage_modal_state = {};
    epaper_ui::SelectModalState select_modal_state = {};
    epaper_ui::KeyboardState keyboard_state = {};
    epaper_ui::ToastState toast_state = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        shutdown_modal_state = s_shutdown_modal_state;
        storage_modal_state = s_storage_modal_state;
        select_modal_state = s_select_modal_state;
        keyboard_state = s_keyboard_state;
        toast_state = s_toast_state;
    }

    return ApplyOverlayState(shutdown_modal_state,
                             storage_modal_state,
                             select_modal_state,
                             keyboard_state,
                             toast_state);
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
        s_storage_modal_state = {};
        s_select_modal_state = {};
        s_keyboard_state = {};
        s_toast_state = {};
        s_overlay_interaction_generation = 1;
        s_feedback_pending = false;
        s_pending_feedback = app_interaction::FeedbackCue::kModalOpen;
        s_shutdown_modal_focus.Configure(0);
        s_storage_modal_focus.Configure(0);
        s_select_modal_focus.Configure(0);
        s_keyboard_event_handler = nullptr;
        s_keyboard_event_context = nullptr;
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

bool IsStorageModalVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_storage_modal_state.visible;
}

bool IsKeyboardVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_keyboard_state.visible;
}

bool IsShutdownPending()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress ||
           s_storage_modal_state.visible || s_select_modal_state.visible ||
           s_keyboard_state.visible;
}

bool IsInputCaptured()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_shutdown_modal_state.visible || s_shutdown_request_in_progress ||
           s_storage_modal_state.visible || s_select_modal_state.visible ||
           s_keyboard_state.visible ||
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
    epaper_ui::StorageModalState storage_modal_state = {};
    epaper_ui::ToastState toast_state = {};
    epaper_ui::KeyboardState keyboard_state = {};
    bool shutdown_visible = false;
    bool storage_visible = false;
    bool select_visible = false;
    bool keyboard_visible = false;
    bool toast_visible = false;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return false;
        }
        shutdown_visible = s_shutdown_modal_state.visible;
        storage_visible = s_storage_modal_state.visible && !s_shutdown_modal_state.visible;
        select_visible = s_select_modal_state.visible && !s_shutdown_modal_state.visible;
        keyboard_visible = s_keyboard_state.visible && !s_shutdown_modal_state.visible &&
                           !storage_visible && !select_visible;
        if (storage_visible) {
            select_visible = false;
        }
        storage_modal_state = s_storage_modal_state;
        keyboard_state = s_keyboard_state;
        toast_visible = s_toast_state.visible;
        select_modal_state = s_select_modal_state;
        toast_state = s_toast_state;
        generation = s_overlay_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();

    if (storage_visible) {
        int action_index = -1;
        bool hit = false;
        action_index = epaper_ui::HitTestStorageModalAction(
            portrait_width, portrait_height, storage_modal_state, x, y, &hit);
        if (hit && action_index >= 0) {
            if (target != nullptr) {
                *target = MakeStorageModalActionTarget(action_index, generation);
            }
            return true;
        }
        return false;
    }

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

    if (keyboard_visible) {
        int flat_key_index = -1;
        if (epaper_ui::HitTestKeyboardKey(portrait_width,
                                          portrait_height,
                                          keyboard_state,
                                          {},
                                          x,
                                          y,
                                          &flat_key_index) &&
            flat_key_index >= 0) {
            if (target != nullptr) {
                *target = MakeKeyboardKeyTarget(flat_key_index, generation);
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
            case app_interaction::Kind::kOverlayStorageModalAction:
                if (!s_storage_modal_state.visible || s_shutdown_modal_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >= StorageModalActionCountLocked()) {
                    return false;
                }
                if (s_storage_modal_state.selected_action_index != target.primary_index) {
                    s_storage_modal_focus.SetIndex(target.primary_index);
                    SyncStorageModalStateFromFocusLocked();
                    changed = true;
                }
                break;
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
            case app_interaction::Kind::kOverlayKeyboardKey:
                if (!s_keyboard_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >=
                        static_cast<int32_t>(epaper_ui::KeyboardKeyCount(s_keyboard_state.layout)) ||
                    s_keyboard_state.selected_key_index == target.primary_index) {
                    return false;
                }
                s_keyboard_state.selected_key_index = target.primary_index;
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
    bool play_click = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    KeyboardEventHandler keyboard_handler = nullptr;
    void* keyboard_context = nullptr;
    epaper_ui::KeyboardState keyboard_callback_state = {};
    epaper_ui::KeyboardIntent keyboard_intent = epaper_ui::KeyboardIntent::kNone;

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
            case app_interaction::Kind::kOverlayStorageModalAction: {
                if (!s_storage_modal_state.visible || s_shutdown_modal_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >= StorageModalActionCountLocked()) {
                    return result;
                }
                s_storage_modal_focus.SetIndex(target.primary_index);
                SyncStorageModalStateFromFocusLocked();
                result.consumed = true;
                if (s_storage_modal_state.kind == epaper_ui::StorageModalKind::kConfirmFormat &&
                    target.primary_index == 1) {
                    result.request_format_sd_card = true;
                }
                s_storage_modal_state = {};
                s_storage_modal_focus.Configure(0);
                AdvanceOverlayInteractionGenerationLocked();
                request_refresh = true;
                play_click = true;
                break;
            }
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
                play_click = true;
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
                } else {
                    play_click = true;
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
                play_click = true;
                break;
            case app_interaction::Kind::kOverlayKeyboardKey: {
                if (!s_keyboard_state.visible ||
                    target.primary_index < 0 ||
                    target.primary_index >=
                        static_cast<int32_t>(epaper_ui::KeyboardKeyCount(s_keyboard_state.layout))) {
                    return result;
                }
                s_keyboard_state.selected_key_index = target.primary_index;
                const epaper_ui::KeyboardActionResult action =
                    epaper_ui::KeyboardController::ActivateFocusedKey(s_keyboard_state, false);
                keyboard_callback_state = s_keyboard_state;
                keyboard_intent = action.intent;
                keyboard_handler = s_keyboard_event_handler;
                keyboard_context = s_keyboard_event_context;
                if (action.intent != epaper_ui::KeyboardIntent::kNone) {
                    s_keyboard_state = {};
                    s_keyboard_event_handler = nullptr;
                    s_keyboard_event_context = nullptr;
                }
                result.consumed = true;
                request_refresh = action.text_changed || action.state_changed ||
                                  action.intent != epaper_ui::KeyboardIntent::kNone;
                play_click = true;
                break;
            }
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

    result.play_feedback = play_click;
    result.feedback_cue = app_interaction::FeedbackCue::kClick;

    if (result.request_format_sd_card) {
        ESP_LOGI(kTag, "Storage modal touch activate: format_confirmed=1");
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayStorageModalAction) {
        ESP_LOGI(kTag, "Storage modal touch activate: format_confirmed=0");
    } else if (result.select_modal_submitted) {
        ESP_LOGI(kTag, "Select modal touch activate index=%d", result.select_modal_selected_index);
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayShutdownAction) {
        ESP_LOGI(kTag,
                 "Shutdown modal touch activate action=%s",
                 result.request_shutdown ? "confirm" : "cancel");
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayToastCloseAction) {
        ESP_LOGI(kTag, "Toast close activated by touch");
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayKeyboardKey) {
        ESP_LOGI(kTag, "Keyboard key activated: index=%ld intent=%d",
                 static_cast<long>(target.primary_index),
                 static_cast<int>(keyboard_intent));
    }

    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true, refresh_policy);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after touch activate failed: %s",
                     esp_err_to_name(err));
        }
    }
    if (keyboard_handler != nullptr) {
        keyboard_handler(keyboard_callback_state, keyboard_intent, keyboard_context);
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
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }
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

esp_err_t ShowStorageModalNoSdCard()
{
    bool changed = false;
    bool play_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        epaper_ui::StorageModalState next_state = {
            .visible = true,
            .kind = epaper_ui::StorageModalKind::kNoSdCard,
            .selected_action_index = 0,
        };
        if (s_storage_modal_state.kind != next_state.kind || !s_storage_modal_state.visible) {
            s_storage_modal_state = next_state;
            s_storage_modal_focus.Configure(1, 0);
            SyncStorageModalStateFromFocusLocked();
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
            play_feedback = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    if (play_feedback) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }
    ESP_LOGI(kTag, "Storage modal shown: no_sd_card");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowStorageModalConfirmFormat()
{
    bool changed = false;
    bool play_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        epaper_ui::StorageModalState next_state = {
            .visible = true,
            .kind = epaper_ui::StorageModalKind::kConfirmFormat,
            .selected_action_index = 0,
        };
        if (s_storage_modal_state.kind != next_state.kind || !s_storage_modal_state.visible ||
            s_storage_modal_state.selected_action_index != next_state.selected_action_index) {
            s_storage_modal_state = next_state;
            s_storage_modal_focus.Configure(2, 0);
            SyncStorageModalStateFromFocusLocked();
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
            play_feedback = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    if (play_feedback) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }
    ESP_LOGI(kTag, "Storage modal shown: confirm_format");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowStorageModalFormatting()
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
        if (!s_storage_modal_state.visible ||
            s_storage_modal_state.kind != epaper_ui::StorageModalKind::kFormatting) {
            s_storage_modal_state.visible = true;
            s_storage_modal_state.kind = epaper_ui::StorageModalKind::kFormatting;
            s_storage_modal_state.selected_action_index = 0;
            s_storage_modal_focus.Configure(0);
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    ESP_LOGI(kTag, "Storage modal shown: formatting");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowStorageModalFormatSuccess()
{
    bool changed = false;
    bool play_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        epaper_ui::StorageModalState next_state = {
            .visible = true,
            .kind = epaper_ui::StorageModalKind::kFormatSuccess,
            .selected_action_index = 0,
        };
        if (s_storage_modal_state.kind != next_state.kind || !s_storage_modal_state.visible) {
            s_storage_modal_state = next_state;
            s_storage_modal_focus.Configure(1, 0);
            SyncStorageModalStateFromFocusLocked();
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
            play_feedback = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    if (play_feedback) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }
    ESP_LOGI(kTag, "Storage modal shown: format_success");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t ShowStorageModalFormatError()
{
    bool changed = false;
    bool play_feedback = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        epaper_ui::StorageModalState next_state = {
            .visible = true,
            .kind = epaper_ui::StorageModalKind::kFormatError,
            .selected_action_index = 0,
        };
        if (s_storage_modal_state.kind != next_state.kind || !s_storage_modal_state.visible) {
            s_storage_modal_state = next_state;
            s_storage_modal_focus.Configure(1, 0);
            SyncStorageModalStateFromFocusLocked();
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
            play_feedback = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    if (play_feedback) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kError);
    }
    ESP_LOGI(kTag, "Storage modal shown: format_error");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t DismissStorageModal()
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
        if (s_storage_modal_state.visible) {
            s_storage_modal_state = {};
            s_storage_modal_focus.Configure(0);
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Storage modal dismissed");
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
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }
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

esp_err_t ShowKeyboard(const epaper_ui::KeyboardState& state,
                       KeyboardEventHandler handler,
                       void* context)
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
        s_keyboard_state = state;
        s_keyboard_state.visible = true;
        epaper_ui::KeyboardController::ClampSelection(s_keyboard_state);
        s_keyboard_event_handler = handler;
        s_keyboard_event_context = context;
        AdvanceOverlayInteractionGenerationLocked();
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
        changed = true;
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Keyboard overlay shown");
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t UpdateKeyboardState(const epaper_ui::KeyboardState& state)
{
    bool changed = false;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        s_keyboard_state = state;
        epaper_ui::KeyboardController::ClampSelection(s_keyboard_state);
        refresh_policy =
            DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
        changed = true;
    }

    if (!changed) {
        return ESP_OK;
    }

    return SyncOverlayState(true, refresh_policy);
}

esp_err_t DismissKeyboard()
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
        if (s_keyboard_state.visible) {
            s_keyboard_state = {};
            s_keyboard_event_handler = nullptr;
            s_keyboard_event_context = nullptr;
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Keyboard overlay dismissed");
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
        if (s_keyboard_state.visible && !s_shutdown_modal_state.visible &&
            !s_storage_modal_state.visible && !s_select_modal_state.visible) {
            if (epaper_ui::KeyboardController::MoveFocus(s_keyboard_state, delta > 0 ? 1 : -1)) {
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        } else if (s_storage_modal_state.visible && !s_shutdown_modal_state.visible) {
            if (s_storage_modal_focus.item_count() <= 0) {
                return false;
            }

            if (s_storage_modal_focus.Move(delta > 0 ? 1 : -1)) {
                SyncStorageModalStateFromFocusLocked();
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        } else if (s_select_modal_state.visible && !s_shutdown_modal_state.visible) {
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

bool TakePendingFeedback(app_interaction::FeedbackCue* cue)
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    if (!s_feedback_pending) {
        return false;
    }
    if (cue != nullptr) {
        *cue = s_pending_feedback;
    }
    s_feedback_pending = false;
    return true;
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
    KeyboardEventHandler keyboard_handler = nullptr;
    void* keyboard_context = nullptr;
    epaper_ui::KeyboardState keyboard_callback_state = {};
    epaper_ui::KeyboardIntent keyboard_callback_intent = epaper_ui::KeyboardIntent::kNone;

    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return result;
        }
        if (s_shutdown_request_in_progress) {
            result.consumed = true;
            return result;
        }
        if (!s_shutdown_modal_state.visible && !s_storage_modal_state.visible &&
            !s_select_modal_state.visible && !s_keyboard_state.visible) {
            return result;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_keyboard_state.visible && !s_shutdown_modal_state.visible &&
            !s_storage_modal_state.visible && !s_select_modal_state.visible) {
            result.consumed = true;
            keyboard_handler = s_keyboard_event_handler;
            keyboard_context = s_keyboard_event_context;
            switch (event.event) {
                case button_service::ButtonEvent::kSingleClick:
                    if (event.button == button_service::ButtonId::kPowerOk) {
                        const epaper_ui::KeyboardActionResult action =
                            epaper_ui::KeyboardController::ActivateFocusedKey(s_keyboard_state, false);
                        keyboard_callback_state = s_keyboard_state;
                        keyboard_callback_intent = action.intent;
                        if (action.intent != epaper_ui::KeyboardIntent::kNone) {
                            s_keyboard_state = {};
                            s_keyboard_event_handler = nullptr;
                            s_keyboard_event_context = nullptr;
                        }
                        request_refresh = action.text_changed || action.state_changed ||
                                          action.intent != epaper_ui::KeyboardIntent::kNone;
                        play_click = true;
                    }
                    break;
                default:
                    break;
            }
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            goto done_locked;
        }

        if (s_storage_modal_state.visible && !s_shutdown_modal_state.visible) {
            result.consumed = true;
            switch (event.event) {
                case button_service::ButtonEvent::kSingleClick:
                    if (event.button == button_service::ButtonId::kPowerOk &&
                        StorageModalActionCountLocked() > 0) {
                        const bool confirmed =
                            s_storage_modal_state.kind ==
                                epaper_ui::StorageModalKind::kConfirmFormat &&
                            s_storage_modal_state.selected_action_index == 1;
                        result.request_format_sd_card = confirmed;
                        s_storage_modal_state = {};
                        s_storage_modal_focus.Configure(0);
                        AdvanceOverlayInteractionGenerationLocked();
                        request_refresh = true;
                        play_click = true;
                        refresh_policy = DetermineOverlayRefreshPolicy(
                            before, CaptureOverlayRefreshSnapshotLocked());
                    }
                    break;
                default:
                    break;
            }
            goto done_locked;
        }

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
                default:
                    break;
            }
            goto done_locked;
        }

        result.consumed = true;
        switch (event.event) {
            case button_service::ButtonEvent::kSingleClick:
                if (event.button == button_service::ButtonId::kPowerOk) {
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
            case button_service::ButtonEvent::kDoubleClick:
            case button_service::ButtonEvent::kPressDown:
            case button_service::ButtonEvent::kPressUp:
            case button_service::ButtonEvent::kLongPressStart:
            case button_service::ButtonEvent::kLongPressUp:
            default:
                break;
        }
    }

done_locked:

    result.play_feedback = play_click;
    result.feedback_cue = app_interaction::FeedbackCue::kClick;
    if (result.consumed &&
        (request_refresh || result.request_format_sd_card) &&
        !dismiss_modal && !dismiss_select_modal &&
        !result.select_modal_submitted) {
        ESP_LOGI(kTag, "Storage modal button action=%s",
                 result.request_format_sd_card ? "confirm" : "dismiss");
    }
    if (dismiss_modal) {
        ESP_LOGI(kTag,
                 "Shutdown modal button action=%s",
                 result.request_shutdown ? "confirm" : "cancel");
    }
    if (dismiss_select_modal) {
        ESP_LOGI(kTag, "Select modal submitted: index=%d", result.select_modal_selected_index);
    }
    if (keyboard_handler != nullptr &&
        (request_refresh || keyboard_callback_intent != epaper_ui::KeyboardIntent::kNone)) {
        ESP_LOGI(kTag, "Keyboard button action intent=%d", static_cast<int>(keyboard_callback_intent));
    }
    if (request_refresh) {
        const esp_err_t err = SyncOverlayState(true, refresh_policy);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(kTag, "Overlay refresh after button failed: %s", esp_err_to_name(err));
        }
    }
    if (keyboard_handler != nullptr) {
        keyboard_handler(keyboard_callback_state, keyboard_callback_intent, keyboard_context);
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
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
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
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(app_interaction::FeedbackCue::kModalOpen);
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
