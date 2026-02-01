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
        // Is this an understandable way to do this?
        enum class SensorHz : uint16_t {
            __78125 = 0x1,
            _1_5625 = 0x2,
            _3_125 = 0x3,
            _6_25 = 0x4,
            _12_5 = 0x5,
            _25 = 0x6,
            _50 = 0x7,
            _100 = 0x8,
            _200 = 0x9,
            _400 = 0xA,
            _800 = 0xB,
            _1600 = 0xC,
            _3200 = 0xD,
            _6400 = 0xE,
        };

        enum class AccelRange : uint16_t {
            _2G = 0x0,
            _4G = 0x1,
            _8G = 0x2,
            _16G = 0x3,
        };

        enum class GyroRange : uint16_t {
            _125DPS = 0x0,
            _250DPS = 0x1,
            _500DPS = 0x2,
            _1000DPS = 0x3,
            _2000DPS = 0x4,
        };

        static constexpr int16_t default_address = 0x68;

        [[nodiscard]]
        static Expected<BMI323> create(I2CDevice&& device);

        BMI323(BMI323&&) = default;
        BMI323& operator=(BMI323&&) = default;
        BMI323(BMI323 const&) = delete;
        BMI323& operator=(BMI323 const&) = delete;

        bool is_connected();

        [[nodiscard]]
        Expected<std::monostate> set_accel_range(AccelRange range);

        [[nodiscard]]
        Expected<std::monostate> set_gyro_range(GyroRange range);

        [[nodiscard]]
        Expected<std::monostate> set_sensor_hz(SensorHz hz);

        [[nodiscard]]
        Expected<IMUData> read_imu();
        
    private:
        explicit BMI323(I2CDevice&& device);

        [[nodiscard]]
        Expected<std::monostate> write_accel_config(AccelRange range, SensorHz hz);

        [[nodiscard]]
        Expected<std::monostate> write_gyro_config(GyroRange range, SensorHz hz);

        I2CDevice device;
        AccelRange accel_range; 
        GyroRange gyro_range;    
        SensorHz sensor_hz;
    };
}