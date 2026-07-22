idf_component_register(
    SRCS
        "audio_codec.cc"
        "board_es8311_codec.cc"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2s esp_driver_gpio esp_driver_i2c esp_codec_dev
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
