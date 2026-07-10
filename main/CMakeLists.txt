idf_component_register(SRCS "onboarding_page_runtime.cpp" "onboarding_page_interactions.cpp" "onboarding_page_coordinator.cpp" "follow_up_page_runtime.cpp" "follow_up_page_interactions.cpp" "follow_up_page_coordinator.cpp" "todos_page_runtime.cpp" "todos_page_interactions.cpp" "todos_page_coordinator.cpp" "details_page_runtime.cpp" "details_page_interactions.cpp" "details_page_coordinator.cpp" "notes_page_runtime.cpp" "notes_page_interactions.cpp" "notes_page_coordinator.cpp" "summarize_page_runtime.cpp" "summarize_page_interactions.cpp" "summarize_page_coordinator.cpp" "vibe_check_page_runtime.cpp" "vibe_check_page_interactions.cpp" "vibe_check_page_coordinator.cpp" "dashboard_page_runtime.cpp" "dashboard_page_interactions.cpp" "dashboard_page_coordinator.cpp" "time_page_runtime.cpp" "time_page_interactions.cpp" "time_page_coordinator.cpp" "page_input_runtime.cpp" "wifi_page_interactions.cpp" "settings_page_interactions.cpp" "input_runtime_setup.cpp" "button_input_runtime.cpp" "input_callback_dispatcher.cpp" "wifi_page_runtime.cpp" "settings_page_runtime.cpp" "settings_page_coordinator.cpp" "timeline_format.cpp" "main.cpp"
                            "app_shell.cpp"
                            "device_sleep_runtime.cpp"
                            "footer_runtime.cpp"
                            "input_focus_runtime.cpp"
                            "lock_screen_runtime.cpp"
                            "overlay_runtime.cpp"
                            "page_interaction_runtime.cpp"
                            "wifi_page_coordinator.cpp"
                            "status_bar_runtime.cpp"
                            "ui_refresh_runtime.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update button_service display_service feedback_service
                                device_sleep_service environment_service epaper_ui esp_driver_gpio
                                esp_hw_support esp_partition gemini_service imu_service power_service
                                nvs_flash page_navigation project_assets recording_archive_service
                                recording_service recording_session_service storage_service
                                summary_service task_config timezone_service touch_service
                                transcription_service wifi_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
