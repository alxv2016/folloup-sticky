idf_component_register(
    SRCS "recording_session_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES feedback_service gemini_service recording_archive_service recording_service storage_service
             transcription_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
