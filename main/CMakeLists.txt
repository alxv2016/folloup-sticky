idf_component_register(SRCS "main.cpp"
                            "app_shell.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update buzzer_service button_service display_service
                                environment_service esp_partition imu_service
                                power_service recording_service storage_service touch_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
