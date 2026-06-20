idf_component_register(
    SRCS
        "generated_epaper_footer_icons.cpp"
        "generated_epaper_icons.cpp"
        "generated_epaper_logos.cpp"
        "project_assets.cpp"
    INCLUDE_DIRS "."
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
