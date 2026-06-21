idf_component_register(
    SRCS "transcription_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls esp_timer gemini_service json recording_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
