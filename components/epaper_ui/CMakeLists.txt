idf_component_register(
    SRCS "status_bar.cpp" "lock_screen.cpp" "shutdown_modal.cpp" "select_modal.cpp" "toast.cpp"
         "global_footer.cpp" "layout_grid.cpp" "button.cpp" "button_icon.cpp" "mic_status.cpp"
         "select_item.cpp" "select_list.cpp" "render_utils.cpp" "generated_epaper_fonts.cpp"
         "bitmap_font.cpp" "font_renderer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES design_tokens project_assets
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
