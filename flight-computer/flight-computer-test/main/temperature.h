#pragma once

#include "driver/i2c_master.h"

extern i2c_master_bus_handle_t tool_bus_handle;

float read_temperature(); 