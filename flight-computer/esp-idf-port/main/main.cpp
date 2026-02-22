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

static const char *TAG = "main";

using namespace seds::errors;

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Hello!");

    auto i2c = seds::I2C::create();
    ESP_LOGI(TAG, "I2C initialized successfully");

    // should we have a template function for calling `create`?

    seds::BMP581 baro_sensor_1 = unwrap(seds::BMP581::create( unwrap(i2c->get_device(seds::BMP581::address_1)) ));
    vTaskDelay(pdMS_TO_TICKS(10)); // otherwise i2c gets angry about NACKs

    seds::BMP581 baro_sensor_2 = unwrap(seds::BMP581::create( unwrap(i2c->get_device(seds::BMP581::address_2)) ));
    vTaskDelay(pdMS_TO_TICKS(10)); 

    //auto disp = seds::TCA6507( unwrap(i2c->get_device(0x45))); //  figure out how this works exactly!
    // remember that we need two busses - one just controls one of the 7-segment devices

    seds::BMI323 imu = unwrap(seds::BMI323::create( unwrap(i2c->get_device(seds::BMI323::default_address)) ));
    if (imu.is_connected()) {
        ESP_LOGI(TAG, "imu connected!");
    } else {
        ESP_LOGE(TAG, "imu not connected!");
    }
   
    //seds::HighGAccel high_g = unwrap(seds::HighGAccel::create( unwrap(i2c->get_device(seds::HighGAccel::default_address)) ));
    //seds::MLX90395 mag_sensor = unwrap(seds::MLX90395::create( unwrap(i2c->get_device(seds::MLX90395::default_address)) ));
    seds::TMP1075 temp_sensor = unwrap(i2c->get_device<seds::TMP1075>());
    if (temp_sensor.is_connected()) {
        ESP_LOGI(TAG, "temp sensor connected!");
    } else {
        ESP_LOGE(TAG, "temp sensor not connected!");
    }

    
    auto data_try = baro_sensor_1.read_data();
    auto data_try_2 = baro_sensor_2.read_data();
    if (!data_try.has_value() || !data_try_2.has_value()) {
        ESP_LOGE(TAG, "baro data read failed");
    } else {
        seds::BarometerData data1 = data_try.value();
        seds::BarometerData data2 = data_try_2.value();
        ESP_LOGI(TAG, "baro 1 temp: %f, baro 2 temp: %f, baro 1 pressure: %f, baro 2 pressure: %f", 
            data1.baro_temp, data2.baro_temp, data1.pressure, data2.pressure);
    }
    

    /*
    seds::SDCard sd = unwrap(seds::SDCard::create());

    auto fc = seds::FlightComputer {
        //.baro1 = std::move(baro_sensor_1),
        //.baro2 = std::move(baro_sensor_2),
        .imu = std::move(imu),
        //.high_g_accel = std::move(high_g),
        //.mag = std::move(mag_sensor),
        .temp = std::move(temp_sensor),
        .sd = std::move(sd)
    };

    auto init_res = fc.init();
    fc.process(10, true);
    */
}
