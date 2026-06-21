idf_component_register(
    SRCS "shared_bus_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_common
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
