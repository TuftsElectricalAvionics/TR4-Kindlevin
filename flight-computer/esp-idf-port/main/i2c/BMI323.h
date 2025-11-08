#pragma once
#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    struct IMUData {
        float ax;
        float ay;
        float az;
        float gx;
        float gy;
        float gz;
    };  

    class BMI323 {
    public:
        static constexpr int16_t default_address = 0x68;

        [[nodiscard]]
        static Result<BMI323> create(I2CDevice&& device);

        bool is_connected();

        [[nodiscard]]
        Result<IMUData> read_imu();
    private:
        explicit BMI323(I2CDevice&& device);

        I2CDevice device;
        // should these be 0 sentinel values instead?
        float accel_sens = 4100.; // LSB/g (actually 4.1 LSB/mg)
        float gyro_sens = 65.536;    // LSB/°/s
    }
}