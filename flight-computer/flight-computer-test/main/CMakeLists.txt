idf_component_register(SRCS "flight_computer_main.c" "i2c_wrapper.c" "temperature.c"
                       PRIV_REQUIRES fatfs esp_driver_i2c
                       INCLUDE_DIRS ".")
