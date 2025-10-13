#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c/I2C.h"
#include "driver/i2c_master.h"
#include "i2c/TMP1075.h"
#include "errors.h"

static const char *TAG = "example";

using namespace seds::errors;

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Hello!");

    auto i2c = seds::I2C::create();
    ESP_LOGI(TAG, "I2C initialized successfully");

    auto temp_sensor = unwrap(i2c->get_device<seds::TMP1075>());

    float const temp = unwrap(temp_sensor.read_temperature());

    ESP_LOGI(TAG, "Temperature: %f", temp);
    std::cout.flush();

    // /* Read the MPU9250 WHO_AM_I register, on power up the register should have the value 0x71 */
    // ESP_ERROR_CHECK(mpu9250_register_read(dev_handle, MPU9250_WHO_AM_I_REG_ADDR, data, 1));
    // ESP_LOGI(TAG, "WHO_AM_I = %X", data[0]);
    //
    // /* Demonstrate writing by resetting the MPU9250 */
    // ESP_ERROR_CHECK(mpu9250_register_write_byte(dev_handle, MPU9250_PWR_MGMT_1_REG_ADDR, 1 << MPU9250_RESET_BIT));
    //
    // ESP_ERROR_CHECK(i2c_master_bus_rm_device(dev_handle));
    // ESP_ERROR_CHECK(i2c_del_master_bus(bus_handle));
    // ESP_LOGI(TAG, "I2C de-initialized successfully");
}
