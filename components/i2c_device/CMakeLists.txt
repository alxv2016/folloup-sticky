idf_component_register(
    SRCS "i2c_device.cc"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
