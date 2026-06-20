idf_component_register(
    SRCS "recording_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES heap microphone_service task_config
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
