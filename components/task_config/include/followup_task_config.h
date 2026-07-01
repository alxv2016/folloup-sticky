#pragma once

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

namespace followup_task_config {

inline constexpr BaseType_t kSystemCore = 0;

#if CONFIG_FREERTOS_UNICORE
inline constexpr BaseType_t kAppCore = 0;
#else
inline constexpr BaseType_t kAppCore = 1;
#endif

inline constexpr UBaseType_t kPriorityRecordCapture = 5;
inline constexpr UBaseType_t kPriorityTouch = 5;
inline constexpr UBaseType_t kPriorityUiRefresh = 4;
inline constexpr UBaseType_t kPriorityAppSleep = 4;
inline constexpr UBaseType_t kPriorityAppShutdown = 4;
inline constexpr UBaseType_t kPriorityDisplay = 3;
inline constexpr UBaseType_t kPrioritySleepMotion = 3;
inline constexpr UBaseType_t kPriorityWifiTransition = 3;
inline constexpr UBaseType_t kPriorityWifiCallbacks = 3;
inline constexpr UBaseType_t kPriorityStorage = 2;
inline constexpr UBaseType_t kPriorityTimezoneSync = 2;
inline constexpr UBaseType_t kPriorityGemini = 2;
// Background battery/RTC telemetry poll. Low priority on purpose: it caches
// last-good values off the UI path, so a poll that loses a race to SD/display
// bus activity simply retries on the next cycle without ever blocking a refresh.
inline constexpr UBaseType_t kPrioritySensorPoll = 2;
// Highest priority on purpose: the buzzer worker only fires a short tone then
// vTaskDelays (it never holds the CPU), so it cannot starve anything. The high
// priority guarantees StopTone runs on time even while a long CPU-bound full-page
// render is in progress -- otherwise a started tone would sustain for the whole
// render (a "continuous beep" on screen transitions).
inline constexpr UBaseType_t kPriorityBuzzer = 6;

}  // namespace followup_task_config
