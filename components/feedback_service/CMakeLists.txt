idf_component_register(SRCS "feedback_service.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES buzzer_service)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
