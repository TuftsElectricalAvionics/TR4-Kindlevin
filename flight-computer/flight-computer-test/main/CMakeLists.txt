idf_component_register(SRCS "temperature.c" "i2ctools_example_main.c"
                            "cmd_i2ctools.c"
                       PRIV_REQUIRES fatfs esp_driver_i2c
                       INCLUDE_DIRS ".")
