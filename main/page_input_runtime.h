#ifndef PAGE_INPUT_RUNTIME_H_
#define PAGE_INPUT_RUNTIME_H_

#include "app_interaction_target.h"
#include "app_interaction_result.h"
#include "button_service.h"
#include "display_service.h"
#include "footer_runtime.h"
#include "page_action_result.h"

namespace page_input_runtime {

struct FocusMoveResult {
    bool handled = false;
    app_interaction::InputResult interaction_result = {};
};

struct ButtonResult {
    bool handled = false;
    app_interaction::InputResult interaction_result = {};
    footer_runtime::FooterFocusItem footer_item = footer_runtime::FooterFocusItem::kNone;
};

footer_runtime::ProjectionState BuildFooterProjectionForScreen(display_service::ScreenId screen);
void ResetFocusForScreen(display_service::ScreenId screen);

FocusMoveResult MoveFocusForCurrentScreen(int delta, bool page_jump = false);
ButtonResult HandleButtonEventForCurrentScreen(const button_service::ButtonEventInfo& event);

}  // namespace page_input_runtime

#endif  // PAGE_INPUT_RUNTIME_H_
