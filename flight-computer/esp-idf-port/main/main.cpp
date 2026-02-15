#include <cstdio>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c/I2C.h"
#include "sd.h"
#include "driver/i2c_master.h"
#include "i2c/TMP1075.h"
#include "errors.h"
#include "task/i2c.h"
#include <esp_pthread.h>

static const char* TAG = "example";

using namespace seds::errors;

extern "C" void app_main() {
    ESP_LOGI(TAG, "Hello!");

    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    esp_pthread_set_cfg(&cfg);

    seds::I2CManager::start();

    while (true) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
