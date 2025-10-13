/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "cmd_i2ctools.h"
#include "driver/i2c_master.h"

static const char *TAG = "i2c-tools";

static gpio_num_t i2c_gpio_sda = CONFIG_EXAMPLE_I2C_MASTER_SDA;
static gpio_num_t i2c_gpio_scl = CONFIG_EXAMPLE_I2C_MASTER_SCL;

static i2c_port_t i2c_port = I2C_NUM_0;

void app_main(void)
{
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .scl_io_num = i2c_gpio_scl,
        .sda_io_num = i2c_gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle));

    printf("\n ==============================================================\n");
    printf(" |             Steps to Use i2c-tools                         |\n");
    printf(" |                                                            |\n");
    printf(" |  1. Try 'help', check all supported commands               |\n");
    printf(" |  2. Try 'i2cconfig' to configure your I2C bus              |\n");
    printf(" |  3. Try 'i2cdetect' to scan devices on the bus             |\n");
    printf(" |  4. Try 'i2cget' to get the content of specific register   |\n");
    printf(" |  5. Try 'i2cset' to set the value of specific register     |\n");
    printf(" |  6. Try 'i2cdump' to dump all the register (Experiment)    |\n");
    printf(" |                                                            |\n");
    printf(" ==============================================================\n\n");

    i2cdetect();
}
