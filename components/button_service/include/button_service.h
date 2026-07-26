#ifndef BUTTON_SERVICE_H_
#define BUTTON_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace button_service {

enum class ButtonId {
    kAction,    // BOOT / GPIO0: recording control, and the light-sleep wake button.
    kFunction,  // FN / GPIO5: activate, double-click, long-press.
    kUp,
    kDown,
};

enum class ButtonEvent {
    kPressDown,
    kPressUp,
    kPressRepeat,
    kSingleClick,
    kDoubleClick,
    kLongPressStart,
    kLongPressUp,
};

// Either of the two non-navigation keys confirms/selects, matching the reference
// firmware where the BOOT and FN buttons both drive primary activation. Recording
// stays exclusive to kAction and the lock screen exclusive to kFunction.
inline bool IsPrimaryButton(ButtonId button)
{
    return button == ButtonId::kAction || button == ButtonId::kFunction;
}

struct ButtonEventInfo {
    ButtonId button = ButtonId::kAction;
    ButtonEvent event = ButtonEvent::kPressDown;
    uint32_t pressed_ms = 0;
};

using EventHandler = void (*)(const ButtonEventInfo& event, void* context);

esp_err_t Init();
void SetEventHandler(EventHandler handler, void* context);

// Stops / restarts the shared iot_button polling timer.
//
// iot_button drives its state machine from a periodic esp_timer that is created
// without `skip_unhandled_events`. Light sleep snaps the esp_timer clock forward by
// the whole sleep duration, so on wake esp_timer replays one callback per missed
// tick in a tight loop -- thousands of state-machine ticks in microseconds, while
// the wake button is still physically held. That instantly pushes the press past
// the long-press threshold and destroys click classification.
//
// Suspend() before entering light sleep and Resume() after the wake pin has been
// restored: esp_timer_start_periodic re-anchors the alarm to "now", so no ticks are
// ever replayed.
esp_err_t Suspend();
esp_err_t Resume();

}  // namespace button_service

#endif  // BUTTON_SERVICE_H_
