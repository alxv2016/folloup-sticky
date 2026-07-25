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
