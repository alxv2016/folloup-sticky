idf_component_register(
    SRCS "timezone_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server esp_timer json nvs_flash power_service
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
