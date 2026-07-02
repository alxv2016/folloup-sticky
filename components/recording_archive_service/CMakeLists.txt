idf_component_register(
    SRCS "recording_archive_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp_timer json nvs_flash recording_service storage_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
