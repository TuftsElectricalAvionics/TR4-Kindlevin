/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* cmd_i2ctools.c

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "argtable3/argtable3.h"
#include "driver/i2c_master.h"
#include "esp_console.h"
#include "esp_log.h"

static const char *TAG = "i2c_wrapper";

typedef struct {
    i2c_master_dev_handle_t handle;
} i2c_device;

#define I2C_TOOL_TIMEOUT_VALUE_MS (50)

// We shouldn't need to chage in this *in* code, bc we can just change this value.
// TODO: Is this a correct assumption?
static uint32_t i2c_frequency = 100 * 1000;
i2c_master_bus_handle_t tool_bus_handle;

static esp_err_t i2c_get_port(int port, i2c_port_t *i2c_port)
{
    if (port >= I2C_NUM_MAX) {
        ESP_LOGE(TAG, "Wrong port number: %d", port);
        return ESP_FAIL;
    }
    *i2c_port = port;
    return ESP_OK;
}

int i2cconfig(int i2c_port, int i2c_gpio_sda, int i2c_gpio_scl)
{
    i2c_port_t i2c_port_checked;
    if (i2c_get_port(i2c_port, &i2c_port_checked) != ESP_OK) {
        return 1;
    }

    // re-init the bus
    if (i2c_del_master_bus(tool_bus_handle) != ESP_OK) {
        return 1;
    }

    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = i2c_port,
        .scl_io_num = i2c_gpio_scl,
        .sda_io_num = i2c_gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    if (i2c_new_master_bus(&i2c_bus_config, &tool_bus_handle) != ESP_OK) {
        return 1;
    }

    return 0;
}

int i2cdetect()
{
    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = i + j;
            esp_err_t ret = i2c_master_probe(tool_bus_handle, address, I2C_TOOL_TIMEOUT_VALUE_MS);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }

    return 0;
}

int register_device(int chip_addr, i2c_device **out) {
    i2c_master_dev_handle_t dev_handle;
    i2c_device_config_t i2c_dev_conf = {
        .scl_speed_hz = i2c_frequency,
        .device_address = chip_addr,
    };
    if (i2c_master_bus_add_device(tool_bus_handle, &i2c_dev_conf, &dev_handle) != ESP_OK) {
        return 1;
    }
    *out = malloc(sizeof(i2c_device));
    (*out)->handle = dev_handle;
    return 0;
}

int free_device(i2c_device **dev) {
    if (i2c_master_bus_rm_device((*dev)->handle) != ESP_OK) {
        return 1;
    }
    free(*dev);
    *dev = NULL;
    return 0;
}

int i2cget(i2c_device *dev, int data_addr, int len)
{
    if (len <= 0) {
        ESP_LOGE(TAG, "No data to read");
        return 1;
    }
    uint8_t *data = malloc(len);

    esp_err_t ret = i2c_master_transmit_receive(dev->handle, (uint8_t*)&data_addr, 1, data, len, I2C_TOOL_TIMEOUT_VALUE_MS);
    if (ret == ESP_OK) {
        for (int i = 0; i < len; i++) {
            printf("0x%02x ", data[i]);
            if ((i + 1) % 16 == 0) {
                printf("\r\n");
            }
        }
        if (len % 16) {
            printf("\r\n");
        }
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        ESP_LOGW(TAG, "Read failed");
    }
    free(data);
    return 0;
}

int i2cset(i2c_device *dev, int data_addr, int *data, int len)
{

    if (len <= 0) {
        ESP_LOGE(TAG, "No data to write");
        return 1;
    }
    if (len > 256) {
        ESP_LOGE(TAG, "Too much data to write");
        return 1;
    }

    uint8_t *data_buf = malloc(len + 1);
    data[0] = data_addr;
    for (int i = 0; i < len; i++) {
        data_buf[i + 1] = data[i];
    }
    esp_err_t ret = i2c_master_transmit(dev->handle, data_buf, len + 1, I2C_TOOL_TIMEOUT_VALUE_MS);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Write OK");
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "Bus is busy");
    } else {
        ESP_LOGW(TAG, "Write Failed");
    }

    free(data);
    return 0;
}

int i2cdump(i2c_device *dev, int size)
{
    if (size != 1 && size != 2 && size != 4) {
        ESP_LOGE(TAG, "Wrong read size. Only support 1,2,4");
        return 1;
    }

    uint8_t data_addr;
    uint8_t data[4];
    int32_t block[16];
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f"
           "    0123456789abcdef\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j += size) {
            fflush(stdout);
            data_addr = i + j;
            esp_err_t ret = i2c_master_transmit_receive(dev->handle, &data_addr, 1, data, size, I2C_TOOL_TIMEOUT_VALUE_MS);
            if (ret == ESP_OK) {
                for (int k = 0; k < size; k++) {
                    printf("%02x ", data[k]);
                    block[j + k] = data[k];
                }
            } else {
                for (int k = 0; k < size; k++) {
                    printf("XX ");
                    block[j + k] = -1;
                }
            }
        }
        printf("   ");
        for (int k = 0; k < 16; k++) {
            if (block[k] < 0) {
                printf("X");
            }
            if ((block[k] & 0xff) == 0x00 || (block[k] & 0xff) == 0xff) {
                printf(".");
            } else if ((block[k] & 0xff) < 32 || (block[k] & 0xff) >= 127) {
                printf("?");
            } else {
                printf("%c", (char)(block[k] & 0xff));
            }
        }
        printf("\r\n");
    }
    return 0;
}