idf_component_register(
    SRCS "qmi8658.cc"
    INCLUDE_DIRS "include"
    REQUIRES i2c_device esp_driver_gpio esp_driver_i2c esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
