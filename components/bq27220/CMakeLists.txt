idf_component_register(
    SRCS "bq27220.cpp"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES esp_driver_i2c
)
