/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct i2c_device i2c_device;

int register_device(int chip_addr, i2c_device **out);
int free_device(i2c_device **dev);

int i2cconfig(int port, int i2c_gpio_sda, int i2c_gpio_scl);
int i2cdetect();
int i2cget(i2c_device *dev, int data_addr, uint8_t *data, int len);
int i2cset(i2c_device *dev, int data_addr, uint8_t *data, int len);
int i2cdump(i2c_device *dev, int size);

extern i2c_master_bus_handle_t tool_bus_handle;

#ifdef __cplusplus
}
#endif
