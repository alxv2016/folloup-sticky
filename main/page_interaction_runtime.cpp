#include "page_interaction_runtime.h"

#include <mutex>

namespace page_interaction_runtime {
namespace {

std::mutex s_provider_mutex;
TouchProvider s_provider = {};

}  // namespace

void RegisterTouchProvider(const TouchProvider& provider)
{
    std::lock_guard<std::mutex> lock(s_provider_mutex);
    s_provider = provider;
}

void ClearTouchProvider()
{
    std::lock_guard<std::mutex> lock(s_provider_mutex);
    s_provider = {};
}

bool HasTouchProvider()
{
    std::lock_guard<std::mutex> lock(s_provider_mutex);
    return s_provider.resolve_touch_target != nullptr &&
           s_provider.focus_touch_target != nullptr &&
           s_provider.activate_touch_target != nullptr;
}

bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target)
{
    TouchProvider provider = {};
    {
        std::lock_guard<std::mutex> lock(s_provider_mutex);
        provider = s_provider;
    }

    if (provider.resolve_touch_target == nullptr) {
        if (target != nullptr) {
            *target = {};
        }
        return false;
    }

    return provider.resolve_touch_target(x, y, target, provider.context);
}

bool FocusTouchTarget(const app_interaction::InteractiveTarget& target)
{
    TouchProvider provider = {};
    {
        std::lock_guard<std::mutex> lock(s_provider_mutex);
        provider = s_provider;
    }

    if (provider.focus_touch_target == nullptr) {
        return false;
    }

    return provider.focus_touch_target(target, provider.context);
}

app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target)
{
    TouchProvider provider = {};
    {
        std::lock_guard<std::mutex> lock(s_provider_mutex);
        provider = s_provider;
    }

    if (provider.activate_touch_target == nullptr) {
        return {};
    }

    return provider.activate_touch_target(target, provider.context);
}

}  // namespace page_interaction_runtime
