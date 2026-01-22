#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "computer/computer.h"
#include "driver/i2c_master.h"
#include "i2c/BMI323.h"
#include "i2c/BMP581.h"
#include "i2c/high_g_accel.h"
#include "i2c/I2C.h"
#include "i2c/MLX90395.h"
#include "i2c/segment7.h"
#include "i2c/TMP1075.h"
#include "errors.h"
#include "sd.h"

static const char *TAG = "example";

using namespace seds::errors;

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Hello!");

    auto i2c = seds::I2C::create();
    ESP_LOGI(TAG, "I2C initialized successfully");

    auto baro_sensor = unwrap(i2c->get_device<seds::BMP581>());
    auto display = unwrap(i2c->get_device<seds::SegmentDisplay>());
    auto imu = unwrap(i2c->get_device<seds::BMI323>()); // should be allowed to fail
    auto high_g = unwrap(i2c->get_device<seds::HighGAccel>());
    auto mag_sensor = unwrap(i2c->get_device<seds::MLX90395>());
    auto temp_sensor = unwrap(i2c->get_device<seds::TMP1075>());

    auto computer = seds::FlightComputer {
        .baro = baro,
        .display = display,
        .imu = imu,
        .high_g_accel = high_g,
        .mag = mag_sensor,
        .temp = temp_sensor
    };

    computer::process();
}
