idf_component_register(
    SRCS "epaper_panel.cpp"
         "ssd1677_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_spi esp_timer heap
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
