#include "temperature.h"
#include "i2c_wrapper.h"

#include "esp_log.h"

static const char *TAG = "temperature";

#define TMP1075_ADDR 0x48   // Address is 48 NOT 49 like on excel sheet
#define TMP1075_TEMP_REG 0x00

i2c_device *temp_sensor = NULL;

int setup_temp_sensor() {
    if (register_device(TMP1075_ADDR, &temp_sensor) != 0) {
        ESP_LOGE(TAG, "Failed to register temperature sensor");
        return -1; 
    }
    // TODO: Sanity check here?
    return 0; 
}

float read_temperature() {
    if (temp_sensor == NULL) {
        ESP_LOGE(TAG, "Temperature sensor not initialized");
        return -273.15; // Return absolute zero as sentinel 
    }

    uint8_t data[2] = {0};  
    if (i2cget(temp_sensor, TMP1075_TEMP_REG, data, 2) != 0) {
        ESP_LOGE(TAG, "Failed to read temperature data");
        return -273.15; // Error reading temperature
    }

    int16_t raw_temp = (data[0] << 8) | data[1];
    raw_temp >>= 4; // TMP1075 gives 12-bit temperature data

    // Convert to Celsius (0.0625°C per LSB)
    return raw_temp * 0.0625;
}