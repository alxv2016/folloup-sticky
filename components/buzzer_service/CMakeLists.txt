idf_component_register(
    SRCS "buzzer_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board esp_driver_ledc task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
