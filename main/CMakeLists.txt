idf_component_register(SRCS "main.cpp"
                            "app_shell.cpp"
                            "device_sleep_runtime.cpp"
                            "footer_runtime.cpp"
                            "lock_screen_runtime.cpp"
                            "overlay_runtime.cpp"
                            "status_bar_runtime.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update button_service display_service feedback_service
                                device_sleep_service environment_service epaper_ui esp_driver_gpio
                                esp_hw_support esp_partition gemini_service imu_service power_service
                                nvs_flash project_assets recording_archive_service
                                recording_service recording_session_service storage_service
                                task_config timezone_service touch_service transcription_service
                                wifi_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
