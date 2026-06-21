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
inline constexpr UBaseType_t kPriorityBuzzer = 2;

}  // namespace followup_task_config
