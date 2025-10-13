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

int i2cconfig(int port, int i2c_frequency, int i2c_gpio_sda, int i2c_gpio_scl);
int i2cdetect();
int i2cget(int chip_addr, int data_addr, int len);
int i2cset(int chip_addr, int data_addr, int *data, int len);
int i2cdump(int chip_addr, int size);

extern i2c_master_bus_handle_t tool_bus_handle;

#ifdef __cplusplus
}
#endif
