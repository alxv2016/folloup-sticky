#ifndef TOUCH_SERVICE_H_
#define TOUCH_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace touch_service {

static constexpr uint8_t kMaxTouchPoints = 5;

enum class TouchPhase : uint8_t {
    kNone = 0,
    kBegin,
    kMove,
    kEnd,
};

struct TouchPoint {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t size = 0;
    uint8_t id = 0;
};

struct TouchEventInfo {
    TouchPhase phase = TouchPhase::kNone;
    uint8_t count = 0;
    TouchPoint points[kMaxTouchPoints] = {};
};

using EventHandler = void (*)(const TouchEventInfo& event, void* context);

esp_err_t Init();
bool IsInitialized();
void SetEventHandler(EventHandler handler, void* context);
esp_err_t RecoverAfterLightSleep();

}  // namespace touch_service

#endif  // TOUCH_SERVICE_H_
