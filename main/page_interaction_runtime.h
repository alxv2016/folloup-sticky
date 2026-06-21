#ifndef PAGE_INTERACTION_RUNTIME_H_
#define PAGE_INTERACTION_RUNTIME_H_

#include "app_interaction_result.h"
#include "app_interaction_target.h"

namespace page_interaction_runtime {

struct TouchProvider {
    // Resolve whether the active page owns a touched target at the provided
    // portrait-space coordinates. Page-local selected indexes should remain
    // render projections of page-owned focus truth rather than touch-only
    // state.
    bool (*resolve_touch_target)(int x,
                                 int y,
                                 app_interaction::InteractiveTarget* target,
                                 void* context) = nullptr;
    // Update page-owned focus truth immediately. Display refresh should remain
    // asynchronous and latest-wins through the page/runtime refresh path.
    bool (*focus_touch_target)(const app_interaction::InteractiveTarget& target,
                               void* context) = nullptr;
    // Perform page-owned activation on touch release for the armed target.
    app_interaction::InputResult (*activate_touch_target)(
        const app_interaction::InteractiveTarget& target, void* context) = nullptr;
    void* context = nullptr;
};

void RegisterTouchProvider(const TouchProvider& provider);
void ClearTouchProvider();
bool HasTouchProvider();
bool ResolveTouchTarget(int x, int y, app_interaction::InteractiveTarget* target);
bool FocusTouchTarget(const app_interaction::InteractiveTarget& target);
app_interaction::InputResult ActivateTouchTarget(const app_interaction::InteractiveTarget& target);

}  // namespace page_interaction_runtime

#endif  // PAGE_INTERACTION_RUNTIME_H_
