idf_component_register(
    SRCS "gemini_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp_http_server esp-tls esp_timer json mbedtls nvs_flash recording_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
