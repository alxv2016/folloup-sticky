idf_component_register(SRCS "main.cpp"
                            "app_shell.cpp"
                            "device_sleep_runtime.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update buzzer_service button_service display_service
                                device_sleep_service environment_service esp_driver_gpio
                                esp_hw_support esp_partition imu_service power_service
                                recording_service storage_service touch_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
