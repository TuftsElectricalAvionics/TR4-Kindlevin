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

    // should we have a template function for calling `create`?

    std::shared_ptr<seds::BMP581> baro_sensor_1 = std::make_shared(unwrap(seds::BMP581::create( unwrap(i2c->get_device(seds::BMP581::address_1)) )));

    
    seds::BMP581 baro_sensor_2 = unwrap(seds::BMP581::create( unwrap(i2c->get_device(seds::BMP581::address_2)) ));

    auto disp = seds::TCA6507( unwrap(i2c->get_device(0x45))); //  figure out how this works exactly!
    // remember that we need two busses - one just controls one of the 7-segment devices

    auto imu = unwrap(seds::BMI323::create( unwrap(i2c->get_device(seds::BMI323::default_address)) ));
    auto high_g = unwrap(seds::HighGAccel::create( unwrap(i2c->get_device(seds::HighGAccel::default_address)) ));
    auto mag_sensor = unwrap(seds::MLX90395::create( unwrap(i2c->get_device(seds::MLX90395::default_address)) ));
    auto temp_sensor = unwrap(i2c->get_device<seds::TMP1075>());

    
    auto computer = seds::FlightComputer {
        .baro1 = baro_sensor_1,
        //.baro2 = baro_sensor_2,
        //.high_g_accel = high_g,
        //.imu = imu,
        //.mag = mag_sensor,
        //.temp = temp_sensor,
    };

    computer::process();
    
}
