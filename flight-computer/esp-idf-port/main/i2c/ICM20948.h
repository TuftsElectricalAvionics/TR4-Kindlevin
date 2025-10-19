#pragma once

#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    /// Should this be broken into seperate accel/gyro structs?
    struct IMUData {
        float ax;
        float ay;
        float az;
        float gx;
        float gy;
        float gz;
    };  

    /// ICM-20948 9-axis IMU.
    class ICM20948 {
    public:  
        static constexpr int16_t default_address = 0x68;

        [[nodiscard]]
        static Expected<ICM20948> create(I2CDevice&& device);

        bool is_connected();

        [[nodiscard]]
        Expected<IMUData> read_imu();

    private:
        explicit ICM20948(I2CDevice&& device);

        I2CDevice device;
    };
};