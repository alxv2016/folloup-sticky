idf_component_register(
    SRCS "display_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board design_tokens epaper_panel project_assets shared_bus_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
