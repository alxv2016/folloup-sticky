idf_component_register(
    SRCS "status_bar.cpp" "lock_screen.cpp" "shutdown_modal.cpp" "toast.cpp"
         "generated_epaper_fonts.cpp" "bitmap_font.cpp" "font_renderer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES design_tokens project_assets
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
