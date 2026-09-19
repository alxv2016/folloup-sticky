#pragma once

#include <cstdint>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

// Creates a long-lived task whose stack is allocated in PSRAM, for workers whose stacks
// (typically HTTPS/TLS callers) do not fit in the scarce internal RAM. `tcb` must live in
// internal RAM for the task's lifetime (e.g. a file-scope StaticTask_t). Returns nullptr on
// failure. Two rules for the task body:
//   - Never delete itself: it has no owner to free the PSRAM stack, and IDF's
//     vTaskDeleteWithCaps self-delete needs an internal-RAM cleanup task and aborts if that
//     cannot be created. Loop on a queue/notification instead.
//   - Never write flash (NVS, partitions): flash writes assert unless the caller's stack is
//     in internal RAM.
inline TaskHandle_t CreatePsramStackTask(TaskFunction_t function,
                                         const char* name,
                                         uint32_t stack_bytes,
                                         void* argument,
                                         UBaseType_t priority,
                                         StaticTask_t* tcb,
                                         BaseType_t core)
{
    auto* stack = static_cast<StackType_t*>(
        heap_caps_malloc(stack_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (stack == nullptr) {
        return nullptr;
    }
    TaskHandle_t task = xTaskCreateStaticPinnedToCore(
        function, name, stack_bytes, argument, priority, stack, tcb, core);
    if (task == nullptr) {
        heap_caps_free(stack);
    }
    return task;
}

}  // namespace followup_task_config
