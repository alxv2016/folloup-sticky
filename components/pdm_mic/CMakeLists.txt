idf_component_register(
    SRCS "pdm_mic.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2s
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
