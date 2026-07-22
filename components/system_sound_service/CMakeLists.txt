idf_component_register(
    SRCS
        "system_sound_service.cc"
        "system_sound_service_catalog.cc"
        "system_sound_service_decoder.cc"
        "system_sound_service_runtime.cc"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES audio_hal
    PRIV_REQUIRES esp_audio_codec esp_timer
    EMBED_FILES
        "sounds/select.mp3"
        "sounds/tap.mp3"
        "sounds/charge.mp3"
        "sounds/complete.mp3"
        "sounds/interrupt.mp3"
        "sounds/lock.mp3"
        "sounds/low_bat.mp3"
        "sounds/shutdown.mp3"
        "sounds/online.mp3"
        "sounds/speaking.mp3"
        "sounds/startup.mp3"
        "sounds/toggle_off.mp3"
        "sounds/toggle_on.mp3"
        "sounds/unlock.mp3"
        "sounds/volume.mp3"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
