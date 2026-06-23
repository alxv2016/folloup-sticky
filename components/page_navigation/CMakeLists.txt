idf_component_register(
    SRCS "page_focus_projection.cpp" "navigation_input_controller.cpp" "navigation_model.cpp" "roving_focus.cpp"
    INCLUDE_DIRS "include"
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
