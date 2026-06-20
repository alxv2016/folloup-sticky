#ifndef DEVICE_SLEEP_SERVICE_H_
#define DEVICE_SLEEP_SERVICE_H_

#include <cstdint>

#include "esp_err.h"

namespace device_sleep_service {

enum class Stage : uint8_t {
    kAwake,
    kDisplaySleeping,
    kLightSleeping,
};

enum class Action : uint8_t {
    kNone,
    kEnterDisplaySleep,
    kWakeDisplay,
    kEnterLightSleep,
    kWakeFromLightSleep,
};

enum class TransitionReason : uint8_t {
    kNone,
    kInactivity,
    kMotion,
    kInteraction,
    kSettingsChange,
};

enum class ActivitySource : uint8_t {
    kUnknown,
    kInteraction,
    kMotion,
};

enum class BlockerReason : uint8_t {
    kNone,
    kRecordingActive,
    kRecordingSaving,
    kShutdownPending,
    kDisplayRefresh,
    kStorageWrite,
    kWifiAccessPoint,
    kTimeSync,
};

struct Settings {
    bool enabled = true;
    uint32_t display_sleep_timeout_seconds = 30;
    uint32_t light_sleep_timeout_seconds = 90;
    bool motion_wake_enabled = true;
    bool interaction_wake_enabled = true;
};

struct RuntimeSnapshot {
    bool initialized = false;
    Stage stage = Stage::kAwake;
    bool inactivity_armed = false;
    uint32_t inactive_seconds = 0;
    uint32_t seconds_until_display_sleep = 0;
    uint32_t seconds_until_light_sleep = 0;
    TransitionReason last_transition_reason = TransitionReason::kNone;
    BlockerReason blocker_reason = BlockerReason::kNone;
    bool blocked = false;
};

struct Snapshot {
    Settings settings = {};
    RuntimeSnapshot runtime = {};
};

struct Event {
    Snapshot snapshot = {};
    Action action = Action::kNone;
    TransitionReason reason = TransitionReason::kNone;
};

using EventHandler = void (*)(const Event& event, void* context);
using BlockerProvider = BlockerReason (*)(void* context);

esp_err_t Init();
esp_err_t ApplySettings(const Settings& settings);
Snapshot GetSnapshot();
void SetEventHandler(EventHandler handler, void* context);
void SetBlockerProvider(BlockerProvider provider, void* context);

bool NotifyNoMotionStarted();
void NotifyUserActivity(ActivitySource source = ActivitySource::kInteraction);
bool NotifyLightSleepWake(TransitionReason reason = TransitionReason::kInteraction);
void NotifyMotionDetected();

const char* StageName(Stage stage);
const char* ActionName(Action action);
const char* TransitionReasonName(TransitionReason reason);
const char* ActivitySourceName(ActivitySource source);
const char* BlockerReasonName(BlockerReason reason);

}  // namespace device_sleep_service

#endif  // DEVICE_SLEEP_SERVICE_H_
