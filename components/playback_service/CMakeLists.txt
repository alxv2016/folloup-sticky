idf_component_register(
    SRCS "playback_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board audio_hal recording_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
