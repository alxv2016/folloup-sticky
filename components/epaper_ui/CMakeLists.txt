idf_component_register(
    SRCS "details_page.cpp" "notes_page.cpp" "timeline_list.cpp" "list_item.cpp" "checkbox.cpp" "summarize_page.cpp" "segment_control.cpp" "scroll_container.cpp" "vibe_check_page.cpp" "vibe_card.cpp" "list_item_header.cpp" "tag.cpp" "card_modal.cpp" "dashboard_page.cpp" "menu_item.cpp" "menu_container.cpp" "completion_banner.cpp" "welcome_message.cpp" "current_date.cpp" "badge.cpp" "time_page.cpp" "time_input.cpp" "select_input.cpp" "text_input.cpp" "wifi_page.cpp" "keyboard.cpp" "keyboard_controller.cpp" "keyboard_layout.cpp" "network_list.cpp" "network_item.cpp" "password_input.cpp" "settings_page.cpp" "menu_toggle.cpp" "sd_status.cpp" "progress_bar.cpp" "toggle.cpp" "status_bar.cpp" "lock_screen.cpp" "select_modal.cpp" "toast.cpp"
         "global_footer.cpp" "layout_grid.cpp" "button.cpp" "button_icon.cpp" "mic_status.cpp"
         "select_item.cpp" "select_list.cpp" "render_utils.cpp" "generated_epaper_fonts.cpp"
         "bitmap_font.cpp" "font_renderer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES design_tokens project_assets
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
