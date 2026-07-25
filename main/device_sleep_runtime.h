#ifndef DEVICE_SLEEP_RUNTIME_H_
#define DEVICE_SLEEP_RUNTIME_H_

#include <cstdint>

#include "button_service.h"
#include "esp_err.h"

namespace device_sleep_runtime {

using ShutdownPendingProvider = bool (*)(void* context);

struct AutoSleepSettings {
    bool enabled = true;
    uint32_t display_sleep_timeout_seconds = 30;
    uint32_t light_sleep_timeout_seconds = 90;
    bool motion_wake_enabled = true;
    bool interaction_wake_enabled = true;
};

void SetShutdownPendingProvider(ShutdownPendingProvider provider, void* context);
esp_err_t StartAutoSleep(const AutoSleepSettings& settings);
esp_err_t StartMotionPolling();
void NotifyUserActivity();

// Marks the POWER_OK press that is waking the device so it only wakes.
//
// Every event that press produces is swallowed by ConsumeWakeOnlyPowerButtonEvent
// until the gesture resolves, with one deliberate exception: DOUBLE_CLICK is let
// through, so waking and toggling the lock screen stays a single gesture. Armed by
// the light-sleep exit path and by the display-sleep wake in app_shell.
void ArmPowerButtonWakeGesture(const char* reason);
bool ConsumeWakeOnlyPowerButtonEvent(const button_service::ButtonEventInfo& event);

}  // namespace device_sleep_runtime

#endif  // DEVICE_SLEEP_RUNTIME_H_
