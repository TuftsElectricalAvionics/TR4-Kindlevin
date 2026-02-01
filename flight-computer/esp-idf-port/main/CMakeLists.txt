idf_component_register(SRCS "sd.cpp" "main.cpp"
        "i2c/I2C.cpp"
        "i2c/TMP1075.cpp"
        PRIV_REQUIRES spi_flash esp_driver_i2c fatfs sdmmc esp_driver_sdspi
        INCLUDE_DIRS ".")
