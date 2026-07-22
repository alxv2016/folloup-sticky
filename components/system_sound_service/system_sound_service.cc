#include "system_sound_service_internal.h"

using system_sound_service_internal::SystemSoundServiceImpl;

SystemSoundService& SystemSoundService::GetInstance() {
    static SystemSoundService instance;
    return instance;
}

void SystemSoundService::Initialize(AudioCodec* codec) {
    SystemSoundServiceImpl::GetInstance().Initialize(codec);
}

void SystemSoundService::PlayCue(SoundCue cue) {
    SystemSoundServiceImpl::GetInstance().PlayCue(cue);
}

void SystemSoundService::PlayCue(SoundCue cue,
                                 std::function<void(SoundCuePlaybackResult)> on_complete) {
    SystemSoundServiceImpl::GetInstance().PlayCue(cue, std::move(on_complete));
}
