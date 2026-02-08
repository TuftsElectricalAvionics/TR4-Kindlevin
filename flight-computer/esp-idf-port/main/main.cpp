#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c/I2C.h"
#include "driver/i2c_master.h"
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

    auto temp_sensor = unwrap(i2c->get_device<seds::TMP1075>());

    std::cout << "Temp sensor connected: " << temp_sensor.is_connected();

    float const temp = unwrap(temp_sensor.read_temperature());

    ESP_LOGI(TAG, "Temperature: %f", temp);
}
