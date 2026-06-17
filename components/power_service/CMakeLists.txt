idf_component_register(
    SRCS "power_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board bq27220 esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
