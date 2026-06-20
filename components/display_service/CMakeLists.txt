idf_component_register(
    SRCS "display_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board epaper_panel
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
