idf_component_register(
    SRCS
        "axp2101.cc"
        "xpowers_axp2101_common.cc"
        "xpowers_axp2101_driver.cc"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_i2c esp_driver_gpio esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
