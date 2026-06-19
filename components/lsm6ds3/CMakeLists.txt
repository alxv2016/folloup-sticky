idf_component_register(
    SRCS "lsm6ds3.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_i2c esp_driver_spi
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
