#pragma once

typedef struct {
    float h_ax;
    float h_ay;
    float h_az;
} HighGAccel;

int setup_accelerometer();
int read_acceleration(HighGAccel *accel_data);

