idf_component_register(
    SRCS "environment_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board sht40
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
