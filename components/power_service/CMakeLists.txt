idf_component_register(
    SRCS "power_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board bq27220 pcf8563 esp_driver_gpio esp_driver_i2c esp_hw_support task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
