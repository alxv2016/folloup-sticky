idf_component_register(
    SRCS "button_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board button
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
