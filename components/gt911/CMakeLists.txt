idf_component_register(
    SRCS "gt911.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2c esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
