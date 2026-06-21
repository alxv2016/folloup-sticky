#ifndef INPUT_FOCUS_RUNTIME_H_
#define INPUT_FOCUS_RUNTIME_H_

#include "button_service.h"
#include "overlay_runtime.h"

namespace input_focus_runtime {

overlay_runtime::InputResult HandleButtonEvent(const button_service::ButtonEventInfo& event);

}  // namespace input_focus_runtime

#endif  // INPUT_FOCUS_RUNTIME_H_
