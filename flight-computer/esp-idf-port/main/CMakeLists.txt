idf_component_register(SRCS "main.cpp"
        "i2c/I2C.cpp"
        "i2c/TMP1075.cpp"
        PRIV_REQUIRES spi_flash esp_driver_i2c esp_driver_uart
        INCLUDE_DIRS ".")
