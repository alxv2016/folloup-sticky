idf_component_register(
    SRCS "touch_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board gt911 esp_driver_gpio
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
