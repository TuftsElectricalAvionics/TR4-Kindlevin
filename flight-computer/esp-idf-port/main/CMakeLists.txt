idf_component_register(SRCS "main.cpp"
        "sd.cpp"
        "i2c/I2C.cpp"
        "i2c/TMP1075.cpp"
        "i2c/high_g_accel.cpp"
        "i2c/segment7.cpp"
        "i2c/BMP581.cpp"
        "i2c/BMI323.cpp"
        "i2c/MLX90395.cpp"
        PRIV_REQUIRES spi_flash esp_driver_i2c esp_timer fatfs sdmmc esp_driver_sdspi
        INCLUDE_DIRS ".")
