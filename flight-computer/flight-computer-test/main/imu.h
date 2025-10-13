#pragma once

// FIXME: Read more than the arduino does?
typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} IMUData;

int setup_imu();
int read_imu(IMUData* data);