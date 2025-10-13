#include "imu.h"
#include "i2c_wrapper.h"

#include "esp_log.h"

static const char *TAG = "imu";

// ------------------- IMU Specifications (Imu: ICM-20948) -------------------
#define ICM_ADDR 0x68  // IMU address
#define PWR_MGMT_1 0x06 // Power management register
#define REG_ACCEL_XOUT_H 0x2D // Accel outputs address
#define REG_ACCEL_YOUT_H REG_ACCEL_XOUT_H + 2
#define REG_ACCEL_ZOUT_H REG_ACCEL_YOUT_H + 2
#define REG_GYRO_XOUT_H  0x33 // Gyro outputs address
#define REG_GYRO_YOUT_H REG_GYRO_XOUT_H + 2
#define REG_GYRO_ZOUT_H REG_GYRO_YOUT_H + 2
// add more registers as needed

// ------------------- Calibrations -------------------

#define ACCEL_SENS 16384.0   // LSB/g
#define GYRO_SENS 32.8       // LSB/°/s

i2c_device *imu_device = NULL;

int setup_imu() {
    if (register_device(ICM_ADDR, &imu_device) != 0) {
        ESP_LOGE(TAG, "Failed to register IMU");
        return 1; 
    }

    uint8_t pwr_data[1] = { 0x01 }; // Set to wake up the IMU
    if (i2cset(imu_device, PWR_MGMT_1, pwr_data, 1) != 0) {
        ESP_LOGE(TAG, "Failed to wake up IMU");
        return 1; 
    }

    // Do we really need to print WHO_AM_I?
    return 0;
}

int read_imu(IMUData* data) {
    if (imu_device == NULL) {
        ESP_LOGE(TAG, "IMU not initialized");
        return 1; 
    }

    uint8_t raw_data[12] = {0}; // 6 bytes for accel, 6 bytes for gyro
    // We can read all at once since they are sequential
    if (i2cget(imu_device, REG_ACCEL_XOUT_H, raw_data, 12) != 0) {
        ESP_LOGE(TAG, "Failed to read IMU data");
        return 1; 
    }

    // Combine high and low bytes for each axis
    int16_t raw_ax = (int16_t)((raw_data[0] << 8) | raw_data[1]);
    int16_t raw_ay = (int16_t)((raw_data[2] << 8) | raw_data[3]);
    int16_t raw_az = (int16_t)((raw_data[4] << 8) | raw_data[5]);
    int16_t raw_gx = (int16_t)((raw_data[6] << 8) | raw_data[7]);
    int16_t raw_gy = (int16_t)((raw_data[8] << 8) | raw_data[9]);
    int16_t raw_gz = (int16_t)((raw_data[10] << 8) | raw_data[11]);

    // Convert to physical units
    data->ax = (float)raw_ax / ACCEL_SENS;
    data->ay = (float)raw_ay / ACCEL_SENS;
    data->az = (float)raw_az / ACCEL_SENS;
    data->gx = (float)raw_gx / GYRO_SENS;
    data->gy = (float)raw_gy / GYRO_SENS;
    data->gz = (float)raw_gz / GYRO_SENS;

    return 0;
}