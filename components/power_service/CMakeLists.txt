idf_component_register(
    SRCS "power_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board axp2101 pcf85063 esp_driver_i2c esp_hw_support task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
