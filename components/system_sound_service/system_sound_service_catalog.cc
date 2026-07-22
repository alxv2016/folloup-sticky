#include "system_sound_service_internal.h"

namespace system_sound_service_internal {
namespace {

// MP3 cue payloads embedded via EMBED_FILES (linker symbols _binary_<name>_mp3_*).
#define SND_DECLARE(name)                                                       \
    extern const uint8_t name##_start[] asm("_binary_" #name "_start");         \
    extern const uint8_t name##_end[] asm("_binary_" #name "_end")

SND_DECLARE(select_mp3);
SND_DECLARE(tap_mp3);
SND_DECLARE(charge_mp3);
SND_DECLARE(complete_mp3);
SND_DECLARE(interrupt_mp3);
SND_DECLARE(lock_mp3);
SND_DECLARE(low_bat_mp3);
SND_DECLARE(shutdown_mp3);
SND_DECLARE(online_mp3);
SND_DECLARE(speaking_mp3);
SND_DECLARE(startup_mp3);
SND_DECLARE(toggle_off_mp3);
SND_DECLARE(toggle_on_mp3);
SND_DECLARE(unlock_mp3);
SND_DECLARE(volume_mp3);
#undef SND_DECLARE

#define SND_SPAN(name) \
    EmbeddedMp3{ name##_start, static_cast<size_t>(name##_end - name##_start) }

}  // namespace

const char* CueName(SoundCue cue) {
    switch (cue) {
    case SoundCue::kNavigationMove:
        return "navigation_move";
    case SoundCue::kButtonActivate:
        return "button_activate";
    case SoundCue::kCharging:
        return "charging";
    case SoundCue::kComplete:
        return "complete";
    case SoundCue::kInterrupt:
        return "interrupt";
    case SoundCue::kLock:
        return "lock";
    case SoundCue::kLowBattery:
        return "low_battery";
    case SoundCue::kModalNotification:
        return "modal_notification";
    case SoundCue::kOnline:
        return "online";
    case SoundCue::kSpeaking:
        return "speaking";
    case SoundCue::kStartup:
        return "startup";
    case SoundCue::kToggleOff:
        return "toggle_off";
    case SoundCue::kToggleOn:
        return "toggle_on";
    case SoundCue::kUnlock:
        return "unlock";
    case SoundCue::kVolume:
        return "volume";
    }
    return "unknown";
}

int64_t CueDebounceWindowUs(SoundCue cue) {
    switch (cue) {
    case SoundCue::kOnline:
        return 1500000;
    case SoundCue::kSpeaking:
    case SoundCue::kInterrupt:
        return 300000;
    case SoundCue::kNavigationMove:
    case SoundCue::kButtonActivate:
    case SoundCue::kCharging:
    case SoundCue::kComplete:
    case SoundCue::kLock:
    case SoundCue::kLowBattery:
    case SoundCue::kModalNotification:
    case SoundCue::kStartup:
    case SoundCue::kToggleOff:
    case SoundCue::kToggleOn:
    case SoundCue::kUnlock:
    case SoundCue::kVolume:
        return 0;
    }
    return 0;
}

EmbeddedMp3 CueMp3(SoundCue cue) {
    switch (cue) {
    case SoundCue::kNavigationMove:
        return SND_SPAN(select_mp3);
    case SoundCue::kButtonActivate:
        return SND_SPAN(tap_mp3);
    case SoundCue::kCharging:
        return SND_SPAN(charge_mp3);
    case SoundCue::kComplete:
        return SND_SPAN(complete_mp3);
    case SoundCue::kInterrupt:
        return SND_SPAN(interrupt_mp3);
    case SoundCue::kLock:
        return SND_SPAN(lock_mp3);
    case SoundCue::kLowBattery:
        return SND_SPAN(low_bat_mp3);
    case SoundCue::kModalNotification:
        return SND_SPAN(shutdown_mp3);
    case SoundCue::kOnline:
        return SND_SPAN(online_mp3);
    case SoundCue::kSpeaking:
        return SND_SPAN(speaking_mp3);
    case SoundCue::kStartup:
        return SND_SPAN(startup_mp3);
    case SoundCue::kToggleOff:
        return SND_SPAN(toggle_off_mp3);
    case SoundCue::kToggleOn:
        return SND_SPAN(toggle_on_mp3);
    case SoundCue::kUnlock:
        return SND_SPAN(unlock_mp3);
    case SoundCue::kVolume:
        return SND_SPAN(volume_mp3);
    }
    return SND_SPAN(tap_mp3);
}

size_t CueIndex(SoundCue cue) {
    return static_cast<size_t>(cue);
}

}  // namespace system_sound_service_internal
