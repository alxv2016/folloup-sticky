idf_component_register(
    SRCS "sd_card.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_driver_sdspi fatfs sdmmc esp_timer
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
