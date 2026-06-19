idf_component_register(
    SRCS
        "storage_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board power_service sd_card fatfs
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
