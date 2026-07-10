#include "onboarding_page_runtime.h"

#include <atomic>
#include <climits>
#include <mutex>

#include "epaper_ui/onboarding_page.h"
#include "esp_log.h"
#include "onboarding_page_coordinator.h"
#include "ui_refresh_runtime.h"

namespace onboarding_page_runtime {
namespace {

constexpr const char* kTag = "OnboardingPageRuntime";

std::mutex s_mutex;
OnboardingPageCoordinator s_coordinator = {};
int32_t s_interaction_generation = 1;
std::atomic<bool> s_pending_dismiss{false};
std::atomic<bool> s_pending_manual_launch{false};

void AdvanceInteractionGenerationLocked()
{
    if (s_interaction_generation == INT32_MAX) {
        s_interaction_generation = 1;
    } else {
        ++s_interaction_generation;
    }
}

epaper_ui::OnboardingPageState BuildStateLocked()
{
    return s_coordinator.BuildState();
}

int32_t ControlIndex(epaper_ui::OnboardingControl control)
{
    return static_cast<int32_t>(control);
}

epaper_ui::OnboardingControl ControlFromIndex(int32_t index)
{
    switch (index) {
        case static_cast<int32_t>(epaper_ui::OnboardingControl::kClose):
            return epaper_ui::OnboardingControl::kClose;
        case static_cast<int32_t>(epaper_ui::OnboardingControl::kPrev):
            return epaper_ui::OnboardingControl::kPrev;
        case static_cast<int32_t>(epaper_ui::OnboardingControl::kNext):
            return epaper_ui::OnboardingControl::kNext;
        default:
            return epaper_ui::OnboardingControl::kNone;
    }
}

const char* ControlName(epaper_ui::OnboardingControl control)
{
    switch (control) {
        case epaper_ui::OnboardingControl::kClose:
            return "Close";
        case epaper_ui::OnboardingControl::kPrev:
            return "Prev";
        case epaper_ui::OnboardingControl::kNext:
            return "Next";
        case epaper_ui::OnboardingControl::kNone:
        default:
            return "MISS";
    }
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetOnboardingPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kOnboardingPage,
                                        &UpdateDisplayState,
                                        display_service::RefreshRequest{.refresh_mode = refresh_mode});
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return onboarding_page_interactions::HandleMoveFocus(s_coordinator, delta);
}

onboarding_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return onboarding_page_interactions::HandlePrimaryActivate(s_coordinator);
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    if (target != nullptr) {
        *target = {};
    }
    epaper_ui::OnboardingPageState state;
    int32_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        state = BuildStateLocked();
        generation = s_interaction_generation;
    }

    const int portrait_width = display_service::PortraitWidth();
    const int portrait_height = display_service::PortraitHeight();
    const epaper_ui::OnboardingControl control =
        epaper_ui::HitTestOnboarding(portrait_width, portrait_height, state, x, y);

    const epaper_ui::CarouselControlRects rects =
        epaper_ui::OnboardingControlBounds(portrait_width, portrait_height, state);
    ESP_LOGI(kTag,
             "Touch resolve: xy=(%d,%d) pw=%d ph=%d hit=%s show_close=%d slide=%d/%d | "
             "close[%d,%d %dx%d] prev[%d,%d %dx%d] next[%d,%d %dx%d]",
             x, y, portrait_width, portrait_height, ControlName(control),
             state.carousel.show_close ? 1 : 0, state.carousel.active_index + 1,
             state.carousel.slide_count, rects.close.x, rects.close.y, rects.close.width,
             rects.close.height, rects.prev.x, rects.prev.y, rects.prev.width, rects.prev.height,
             rects.next.x, rects.next.y, rects.next.width, rects.next.height);

    if (control == epaper_ui::OnboardingControl::kNone) {
        return false;
    }
    if (target != nullptr) {
        *target = {
            .owner = app_interaction::Owner::kPage,
            .kind = app_interaction::Kind::kPageAction,
            .primary_index = ControlIndex(control),
            .secondary_index = generation,
        };
    }
    return true;
}

page_actions::FocusUpdateOutcome FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    page_actions::FocusUpdateOutcome result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    const bool generation_ok = target.secondary_index == s_interaction_generation;
    const epaper_ui::OnboardingControl control = ControlFromIndex(target.primary_index);
    if (!generation_ok) {
        ESP_LOGW(kTag, "Touch focus stale: control=%s target_gen=%ld cur_gen=%ld",
                 ControlName(control), static_cast<long>(target.secondary_index),
                 static_cast<long>(s_interaction_generation));
        return result;
    }
    const bool focused = s_coordinator.FocusControl(control);
    ESP_LOGI(kTag, "Touch focus: control=%s focused=%d", ControlName(control), focused ? 1 : 0);
    if (!focused) {
        return result;
    }
    result.handled = true;
    result.apply_page_state = true;
    return result;
}

onboarding_page_interactions::ActivateResult ActivateTouchTarget(
    const app_interaction::InteractiveTarget& target)
{
    onboarding_page_interactions::ActivateResult result = {};
    if (target.owner != app_interaction::Owner::kPage) {
        return result;
    }
    std::lock_guard<std::mutex> lock(s_mutex);
    if (target.secondary_index != s_interaction_generation) {
        ESP_LOGW(kTag, "Touch activate stale: control=%s target_gen=%ld cur_gen=%ld",
                 ControlName(ControlFromIndex(target.primary_index)),
                 static_cast<long>(target.secondary_index),
                 static_cast<long>(s_interaction_generation));
        return result;
    }
    // Touch-down already focused the control; the release runs the normal activation. Capture the
    // tapped control and the slide index BEFORE activation -- HandlePrimaryActivate moves focus (e.g.
    // reaching the last slide auto-focuses Close), so reading focus afterwards mislabels the action.
    const epaper_ui::OnboardingControl activated = ControlFromIndex(target.primary_index);
    const int slide_before = s_coordinator.active_index() + 1;
    result = onboarding_page_interactions::HandlePrimaryActivate(s_coordinator);
    ESP_LOGI(kTag, "Touch activate: control=%s handled=%d intent=%d slide=%d->%d/%d",
             ControlName(activated), result.handled ? 1 : 0, static_cast<int>(result.intent),
             slide_before, s_coordinator.active_index() + 1, s_coordinator.slide_count());
    return result;
}

void ResetFocus()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    s_coordinator.PrepareForShow();
    AdvanceInteractionGenerationLocked();
}

void RequestDismiss()
{
    s_pending_dismiss.store(true, std::memory_order_relaxed);
}

bool ConsumePendingDismiss()
{
    return s_pending_dismiss.exchange(false, std::memory_order_relaxed);
}

void RequestManualLaunch()
{
    s_pending_manual_launch.store(true, std::memory_order_relaxed);
}

bool ConsumePendingManualLaunch()
{
    return s_pending_manual_launch.exchange(false, std::memory_order_relaxed);
}

}  // namespace onboarding_page_runtime
