idf_component_register(
    SRCS
        "storage_service.cpp"
        "usb_storage_backend.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board power_service sd_card fatfs task_config esp_timer
             esp_driver_sdmmc sdmmc espressif__esp_tinyusb
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
