idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES app_update esp_partition power_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
