idf_component_register(
    SRCS "pcf8563.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
