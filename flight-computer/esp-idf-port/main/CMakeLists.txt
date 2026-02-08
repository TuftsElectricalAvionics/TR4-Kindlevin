idf_component_register(SRCS "sd.cpp" "main.cpp"
        "i2c/I2C.cpp"
        "i2c/TMP1075.cpp"
        "i2c/high_g_accel.cpp"
        "i2c/segment7.cpp"
        "i2c/BMP581.cpp"
        "i2c/BMI323.cpp"
        "i2c/MLX90395.cpp"
        "sd.cpp"
        PRIV_REQUIRES spi_flash esp_driver_i2c fatfs esp_timer
        INCLUDE_DIRS ".")
