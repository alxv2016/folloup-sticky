idf_component_register(
    SRCS "display_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board design_tokens epaper_panel epaper_ui project_assets
             task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
