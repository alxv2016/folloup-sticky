idf_component_register(
    SRCS "imu_service.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board qmi8658
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
