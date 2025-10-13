#include "accelerometer.h"
#include "i2c_wrapper.h"

#include "esp_log.h"

static const char *TAG = "accelerometer";

#define ACCEL_ADDRESS 0x53  // ADXL375 I2C address
#define REG_DATA_FORMAT 0x31 
#define REG_DATAX0 0x32
#define REG_DATAY0 REG_DATAX0 + 2
#define REG_DATAZ0 REG_DATAY0 + 2
#define RESOLUTION 0x0F // Full resolution, ±200g 
#define HIGH_G_ACCEL_SENS 20.5 //I'm a little unsure about this value
#define REG_POWER_CTL 0x2D

#define MEASURE_BIT 0x08

i2c_device *accel_device = NULL;

int setup_accelerometer() {
    if (register_device(ACCEL_ADDRESS, &accel_device) != 0) {
        ESP_LOGE(TAG, "Failed to register accelerometer");
        return 1; 
    }

    // Set resolution
    uint8_t resolution_data[1] = {RESOLUTION};
    if (i2cset(accel_device, REG_DATA_FORMAT, resolution_data, 1) != 0) {
        ESP_LOGE(TAG, "Failed to set resolution");
        return 1; 
    }

    // Enable measurements
    uint8_t measure_data[1] = {MEASURE_BIT};
    if (i2cset(accel_device, REG_POWER_CTL, measure_data, 1) != 0) {
        ESP_LOGE(TAG, "Failed to enable measurements");
        return 1; 
    }

    return 0; 
}

int16_t read_axis(uint8_t reg) {
    uint8_t data[2] = {0};
    if (i2cget(accel_device, reg, data,2) != 0) {
        ESP_LOGE(TAG, "Failed to read axis data");
        return 0; 
    }
    return (int16_t)((data[1] << 8) | data[0]);
}

int read_acceleration(HighGAccel *accel_data) {
    if (accel_device == NULL) {
        ESP_LOGE(TAG, "Accelerometer not initialized");
        return 1; 
    }

    int16_t raw_x = read_axis(REG_DATAX0);
    int16_t raw_y = read_axis(REG_DATAY0);
    int16_t raw_z = read_axis(REG_DATAZ0);

    // Convert to g's
    accel_data->h_ax = (float)raw_x / HIGH_G_ACCEL_SENS;
    accel_data->h_ay = (float)raw_y / HIGH_G_ACCEL_SENS;
    accel_data->h_az = (float)raw_z / HIGH_G_ACCEL_SENS;

    return 0; 
}