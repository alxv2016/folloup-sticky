#include "overlay_runtime.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

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

enum class CardModalPurpose : uint8_t {
    kNone,
    kShutdownConfirm,
    kStorageNoSdCard,
    kStorageConfirmFormat,
    kStorageFormatting,
    kStorageFormatSuccess,
    kStorageFormatError,
};

std::mutex s_state_mutex;
bool s_initialized = false;
epaper_ui::CardModalState s_card_modal_state = {};
CardModalPurpose s_card_modal_purpose = CardModalPurpose::kNone;
epaper_ui::SelectModalState s_select_modal_state = {};
epaper_ui::KeyboardState s_keyboard_state = {};
epaper_ui::ToastState s_toast_state = {};
page_navigation::RovingFocus s_card_modal_focus = {};
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

epaper_ui::CardModalState BuildCardModalState(CardModalPurpose purpose)
{
    epaper_ui::CardModalState state = {};
    state.visible = true;
    switch (purpose) {
        case CardModalPurpose::kShutdownConfirm:
            state.title_text = "Shut down device?";
            state.body_text = "Your device will power off. Do you want to continue?";
            state.action_labels = {"Cancel", "Shut down"};
            break;
        case CardModalPurpose::kStorageNoSdCard:
            state.title_text = "No SD card";
            state.body_text = "No SD card is inserted. Insert an SD card to continue.";
            state.action_labels = {"OK"};
            break;
        case CardModalPurpose::kStorageConfirmFormat:
            state.title_text = "Format SD card?";
            state.body_text = "Formatting the SD card will erase everything on the card.";
            state.action_labels = {"Cancel", "Format"};
            break;
        case CardModalPurpose::kStorageFormatting:
            state.title_text = "Formatting SD card";
            state.body_text = "Formatting in progress. Please wait...";
            state.action_labels = {};
            break;
        case CardModalPurpose::kStorageFormatSuccess:
            state.title_text = "Format success";
            state.body_text = "The SD card was formatted successfully.";
            state.action_labels = {"OK"};
            break;
        case CardModalPurpose::kStorageFormatError:
            state.title_text = "Format failed";
            state.body_text = "There was an error and the SD card could not be formatted.";
            state.action_labels = {"OK"};
            break;
        case CardModalPurpose::kNone:
        default:
            state.visible = false;
            break;
    }
    return state;
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

app_interaction::InteractiveTarget MakeCardModalActionTarget(int index, int32_t generation)
{
    return {
        .owner = app_interaction::Owner::kOverlay,
        .kind = app_interaction::Kind::kOverlayCardModalAction,
        .primary_index = index,
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

void SyncCardModalStateFromFocusLocked()
{
    s_card_modal_state.selected_action_index = std::max(0, s_card_modal_focus.index());
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
    bool card_modal_visible = false;
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

    snapshot.card_modal_visible = s_card_modal_state.visible;
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
    if (snapshot.select_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::SelectModalPanelBounds(
                    portrait_width, portrait_height, s_select_modal_state),
                design::modal::kShadowOffset));
    }
    if (snapshot.card_modal_visible) {
        snapshot.bounds = epaper_ui::UnionRect(
            snapshot.bounds,
            ShadowedRect(
                epaper_ui::CardModalPanelBounds(
                    portrait_width, portrait_height, s_card_modal_state),
                design::modal::kShadowOffset));
    }

    return snapshot;
}

display_service::OverlayRefreshPolicy DetermineOverlayRefreshPolicy(
    const OverlayRefreshSnapshot& before, const OverlayRefreshSnapshot& after)
{
    // Dismissing a LARGE overlay (keyboard, select list) needs a full refresh:
    // clearing that much high-contrast area with a partial refresh leaves e-paper
    // ghosting even now that SD I/O is serialized with the display on the shared
    // bus. (The bus serialization fixed the *other* ghosting cause -- SD writes
    // cutting into a partial refresh mid-transaction -- which is why small
    // overlays clear fine with partial.) This is the single, central place that
    // decides full-vs-partial on dismiss, by overlay type, so pages don't each
    // choose (and drift).
    const bool large_overlay_dismissed =
        (before.keyboard_visible && !after.keyboard_visible) ||
        (before.select_visible && !after.select_visible);
    if (large_overlay_dismissed) {
        return display_service::OverlayRefreshPolicy::kRebuildUnderlayFull;
    }

    // Any other visibility change (overlay shown or moved, or a small card
    // modal / toast dismissed) rebuilds the underlay with a partial refresh.
    const bool visibility_changed = before.card_modal_visible != after.card_modal_visible ||
                                    before.select_visible != after.select_visible ||
                                    before.keyboard_visible != after.keyboard_visible ||
                                    before.toast_visible != after.toast_visible;
    if (visibility_changed || !RectEquals(before.bounds, after.bounds)) {
        return display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    }

    return display_service::OverlayRefreshPolicy::kReuseUnderlaySnapshot;
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

esp_err_t ApplyOverlayState(const epaper_ui::CardModalState& card_modal_state,
                            const epaper_ui::SelectModalState& select_modal_state,
                            const epaper_ui::KeyboardState& keyboard_state,
                            const epaper_ui::ToastState& toast_state)
{
    ESP_RETURN_ON_ERROR(display_service::SetCardModalState(card_modal_state),
                        kTag,
                        "set card modal state failed");
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

    epaper_ui::CardModalState card_modal_state = {};
    epaper_ui::SelectModalState select_modal_state = {};
    epaper_ui::KeyboardState keyboard_state = {};
    epaper_ui::ToastState toast_state = {};
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        card_modal_state = s_card_modal_state;
        select_modal_state = s_select_modal_state;
        keyboard_state = s_keyboard_state;
        toast_state = s_toast_state;
    }

    return ApplyOverlayState(card_modal_state,
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
        s_card_modal_state = {};
        s_card_modal_purpose = CardModalPurpose::kNone;
        s_select_modal_state = {};
        s_keyboard_state = {};
        s_toast_state = {};
        s_overlay_interaction_generation = 1;
        s_feedback_pending = false;
        s_pending_feedback = app_interaction::FeedbackCue::kModalOpen;
        s_card_modal_focus.Configure(0);
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
    return s_card_modal_state.visible &&
           s_card_modal_purpose == CardModalPurpose::kShutdownConfirm;
}

bool IsStorageModalVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_card_modal_state.visible &&
           (s_card_modal_purpose == CardModalPurpose::kStorageNoSdCard ||
            s_card_modal_purpose == CardModalPurpose::kStorageConfirmFormat ||
            s_card_modal_purpose == CardModalPurpose::kStorageFormatting ||
            s_card_modal_purpose == CardModalPurpose::kStorageFormatSuccess ||
            s_card_modal_purpose == CardModalPurpose::kStorageFormatError);
}

bool IsKeyboardVisible()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_keyboard_state.visible;
}

bool IsShutdownPending()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_card_modal_state.visible || s_shutdown_request_in_progress ||
           s_select_modal_state.visible || s_keyboard_state.visible;
}

bool IsInputCaptured()
{
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return s_card_modal_state.visible || s_shutdown_request_in_progress ||
           s_select_modal_state.visible || s_keyboard_state.visible ||
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
    epaper_ui::CardModalState card_modal_state = {};
    epaper_ui::ToastState toast_state = {};
    epaper_ui::KeyboardState keyboard_state = {};
    bool card_modal_visible = false;
    bool select_visible = false;
    bool keyboard_visible = false;
    bool toast_visible = false;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized || s_shutdown_request_in_progress) {
            return false;
        }
        card_modal_visible = s_card_modal_state.visible;
        select_visible = s_select_modal_state.visible && !card_modal_visible;
        keyboard_visible = s_keyboard_state.visible && !card_modal_visible && !select_visible;
        card_modal_state = s_card_modal_state;
        keyboard_state = s_keyboard_state;
        toast_visible = s_toast_state.visible;
        select_modal_state = s_select_modal_state;
        toast_state = s_toast_state;
        generation = s_overlay_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();

    if (card_modal_visible) {
        bool hit = false;
        const int action_index = epaper_ui::HitTestCardModalAction(
            portrait_width, portrait_height, card_modal_state, x, y, &hit);
        if (hit && action_index >= 0) {
            if (target != nullptr) {
                *target = MakeCardModalActionTarget(action_index, generation);
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
            case app_interaction::Kind::kOverlayCardModalAction:
                if (!s_card_modal_state.visible || target.primary_index < 0 ||
                    target.primary_index >=
                        epaper_ui::CardModalActionCount(s_card_modal_state)) {
                    return false;
                }
                if (s_card_modal_state.selected_action_index != target.primary_index) {
                    s_card_modal_focus.SetIndex(target.primary_index);
                    SyncCardModalStateFromFocusLocked();
                    changed = true;
                }
                break;
            case app_interaction::Kind::kOverlaySelectModalItem:
                if (!s_select_modal_state.visible || s_card_modal_state.visible ||
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
                        static_cast<int32_t>(epaper_ui::KeyboardKeyCount(s_keyboard_state.layout))) {
                    return false;
                }
                // Touch-down on a keyboard key: report the focus so the press haptic fires,
                // but do NOT commit selected_key_index or refresh here. The pressed-key
                // highlight must appear only together with the typed character on activate
                // (touch-up) — committing it on touch-down would let any independent overlay
                // refresh (toast timer, status update) repaint the highlight before the
                // character is typed. ActivateTouchTarget sets selected_key_index and
                // refreshes on release; input_focus_runtime dedups the haptic across move
                // events via focused_target, so returning true here is safe.
                return true;
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
            case app_interaction::Kind::kOverlayCardModalAction: {
                if (!s_card_modal_state.visible || target.primary_index < 0 ||
                    target.primary_index >=
                        epaper_ui::CardModalActionCount(s_card_modal_state)) {
                    return result;
                }
                s_card_modal_focus.SetIndex(target.primary_index);
                SyncCardModalStateFromFocusLocked();
                result.consumed = true;
                switch (s_card_modal_purpose) {
                    case CardModalPurpose::kShutdownConfirm:
                        if (target.primary_index == 1) {
                            s_shutdown_request_in_progress = true;
                            result.request_shutdown = true;
                        } else {
                            play_click = true;
                        }
                        break;
                    case CardModalPurpose::kStorageConfirmFormat:
                        if (target.primary_index == 1) {
                            result.request_format_sd_card = true;
                        } else {
                            play_click = true;
                        }
                        break;
                    case CardModalPurpose::kStorageNoSdCard:
                    case CardModalPurpose::kStorageFormatSuccess:
                    case CardModalPurpose::kStorageFormatError:
                        play_click = true;
                        break;
                    case CardModalPurpose::kStorageFormatting:
                    case CardModalPurpose::kNone:
                    default:
                        break;
                }
                s_card_modal_state = {};
                s_card_modal_purpose = CardModalPurpose::kNone;
                s_card_modal_focus.Configure(0);
                AdvanceOverlayInteractionGenerationLocked();
                request_refresh = true;
                break;
            }
            case app_interaction::Kind::kOverlaySelectModalItem:
                if (!s_select_modal_state.visible || s_card_modal_state.visible ||
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

    if (result.select_modal_submitted) {
        ESP_LOGI(kTag, "Select modal touch activate index=%d", result.select_modal_selected_index);
    } else if (result.consumed &&
               target.kind == app_interaction::Kind::kOverlayCardModalAction) {
        ESP_LOGI(kTag,
                 "Card modal touch activate action=%ld",
                 static_cast<long>(target.primary_index));
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

namespace {

esp_err_t ShowCardModal(CardModalPurpose purpose, const char* log_label)
{
    bool changed = false;
    bool play_feedback = false;
    app_interaction::FeedbackCue feedback_cue = app_interaction::FeedbackCue::kModalOpen;
    display_service::OverlayRefreshPolicy refresh_policy =
        display_service::OverlayRefreshPolicy::kRebuildUnderlay;
    {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        if (!s_initialized) {
            return ESP_ERR_INVALID_STATE;
        }
        if (s_card_modal_purpose != purpose || !s_card_modal_state.visible) {
            const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
            s_card_modal_state = BuildCardModalState(purpose);
            s_card_modal_purpose = purpose;
            s_card_modal_focus.Configure(
                epaper_ui::CardModalActionCount(s_card_modal_state), 0);
            s_card_modal_state.selected_action_index = std::max(0, s_card_modal_focus.index());
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
            // The formatting card is a transient progress state; the original
            // ShowStorageModalFormatting did NOT queue any feedback. Every other
            // purpose queues kModalOpen, except the format-error card which
            // queued kError.
            play_feedback = purpose != CardModalPurpose::kStorageFormatting;
            feedback_cue = purpose == CardModalPurpose::kStorageFormatError
                               ? app_interaction::FeedbackCue::kError
                               : app_interaction::FeedbackCue::kModalOpen;
        }
    }

    if (!changed) {
        return ESP_OK;
    }
    if (play_feedback) {
        std::lock_guard<std::mutex> lock(s_state_mutex);
        QueuePendingFeedbackLocked(feedback_cue);
    }
    ESP_LOGI(kTag, "Card modal shown: %s", log_label);
    return SyncOverlayState(true, refresh_policy);
}

esp_err_t DismissCardModal()
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
        if (s_card_modal_state.visible) {
            s_card_modal_state = {};
            s_card_modal_purpose = CardModalPurpose::kNone;
            s_card_modal_focus.Configure(0);
            AdvanceOverlayInteractionGenerationLocked();
            refresh_policy =
                DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
            changed = true;
        }
    }

    if (!changed) {
        return ESP_OK;
    }

    ESP_LOGI(kTag, "Card modal dismissed");
    return SyncOverlayState(true, refresh_policy);
}

}  // namespace

esp_err_t ShowShutdownModal()
{
    return ShowCardModal(CardModalPurpose::kShutdownConfirm, "shutdown_confirm");
}

esp_err_t DismissShutdownModal()
{
    return DismissCardModal();
}

esp_err_t ShowStorageModalNoSdCard()
{
    return ShowCardModal(CardModalPurpose::kStorageNoSdCard, "no_sd_card");
}

esp_err_t ShowStorageModalConfirmFormat()
{
    return ShowCardModal(CardModalPurpose::kStorageConfirmFormat, "confirm_format");
}

esp_err_t ShowStorageModalFormatting()
{
    return ShowCardModal(CardModalPurpose::kStorageFormatting, "formatting");
}

esp_err_t ShowStorageModalFormatSuccess()
{
    return ShowCardModal(CardModalPurpose::kStorageFormatSuccess, "format_success");
}

esp_err_t ShowStorageModalFormatError()
{
    return ShowCardModal(CardModalPurpose::kStorageFormatError, "format_error");
}

esp_err_t DismissStorageModal()
{
    return DismissCardModal();
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
        if (s_card_modal_state.visible) {
            if (s_card_modal_focus.item_count() <= 0) {
                return false;
            }

            if (s_card_modal_focus.Move(delta > 0 ? 1 : -1)) {
                SyncCardModalStateFromFocusLocked();
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        } else if (s_keyboard_state.visible && !s_select_modal_state.visible) {
            if (epaper_ui::KeyboardController::MoveFocus(s_keyboard_state, delta > 0 ? 1 : -1)) {
                refresh_policy =
                    DetermineOverlayRefreshPolicy(before, CaptureOverlayRefreshSnapshotLocked());
                changed = true;
            }
        } else if (s_select_modal_state.visible) {
            if (s_select_modal_focus.item_count() <= 0) {
                return false;
            }

            if (s_select_modal_focus.Move(delta > 0 ? 1 : -1)) {
                SyncSelectModalStateFromFocusLocked();
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
        if (!s_card_modal_state.visible && !s_select_modal_state.visible &&
            !s_keyboard_state.visible) {
            return result;
        }

        const OverlayRefreshSnapshot before = CaptureOverlayRefreshSnapshotLocked();
        if (s_card_modal_state.visible) {
            result.consumed = true;
            switch (event.event) {
                case button_service::ButtonEvent::kSingleClick:
                    if (event.button == button_service::ButtonId::kPowerOk &&
                        epaper_ui::CardModalActionCount(s_card_modal_state) > 0) {
                        const int index = s_card_modal_state.selected_action_index;
                        switch (s_card_modal_purpose) {
                            case CardModalPurpose::kShutdownConfirm:
                                if (index == 1) {
                                    s_shutdown_request_in_progress = true;
                                    result.request_shutdown = true;
                                } else {
                                    play_click = true;
                                }
                                dismiss_modal = true;
                                break;
                            case CardModalPurpose::kStorageConfirmFormat:
                                if (index == 1) {
                                    result.request_format_sd_card = true;
                                } else {
                                    play_click = true;
                                }
                                break;
                            case CardModalPurpose::kStorageNoSdCard:
                            case CardModalPurpose::kStorageFormatSuccess:
                            case CardModalPurpose::kStorageFormatError:
                                play_click = true;
                                break;
                            case CardModalPurpose::kStorageFormatting:
                            case CardModalPurpose::kNone:
                            default:
                                break;
                        }
                        s_card_modal_state = {};
                        s_card_modal_purpose = CardModalPurpose::kNone;
                        s_card_modal_focus.Configure(0);
                        AdvanceOverlayInteractionGenerationLocked();
                        request_refresh = true;
                        refresh_policy = DetermineOverlayRefreshPolicy(
                            before, CaptureOverlayRefreshSnapshotLocked());
                    }
                    break;
                default:
                    break;
            }
            goto done_locked;
        }

        if (s_keyboard_state.visible && !s_select_modal_state.visible) {
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

        if (s_select_modal_state.visible) {
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
    }

done_locked:

    result.play_feedback = play_click;
    result.feedback_cue = app_interaction::FeedbackCue::kClick;
    if (result.consumed &&
        (request_refresh || result.request_format_sd_card) &&
        !dismiss_modal && !dismiss_select_modal &&
        !result.select_modal_submitted) {
        ESP_LOGI(kTag, "Card modal button action=%s",
                 result.request_format_sd_card ? "confirm" : "dismiss");
    }
    if (dismiss_modal) {
        ESP_LOGI(kTag,
                 "Card modal button action=%s",
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
