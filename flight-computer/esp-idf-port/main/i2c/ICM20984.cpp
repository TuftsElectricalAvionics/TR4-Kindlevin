#include "ICM20948.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

namespace seds {
    enum class ICM20984Register : uint8_t {
        WHO_AM_I = 0x00,
        PWR_MGMT_1 = 0x06,
        ACCEL_XOUT_H = 0x2D,
        ACCEL_YOUT_H = 0x2F,
        ACCEL_ZOUT_H = 0x31,
        GYRO_XOUT_H = 0x33,
        GYRO_YOUT_H = 0x35,
        GYRO_ZOUT_H = 0x37,
    };

    ICM20948::ICM20948(I2CDevice&& device) : device(std::move(device)) {}

    Expected<ICM20948> ICM20948::create(I2CDevice&& device) {
        ICM20948 imu(std::move(device));

        /// FIXME: Is this the proper initialization procedure?
        TRY(
            imu.device.write_be_register<uint8_t>(
                ICM20984Register::PWR_MGMT_1,
                0x01 // Set clock source
            )
        )

        return imu;
    }

    bool ICM20948::is_connected() {
        constexpr uint8_t expected_who_am_i = 0xEA;

        auto result = this->device.read_be_register<uint8_t>(ICM20984Register::WHO_AM_I);
        return result.has_value() && result.value() == expected_who_am_i;
    }

    Expected<IMUData> ICM20948::read_imu() {
        constexpr float ACCEL_SENS = 16384.0;   // LSB/g
        constexpr float GYRO_SENS = 32.8;       // LSB/°/s

        /// These are stored in big-endian format   
        /// This has to be done one at a time to convert from big endian
        auto ax = TRY(this->device.read_be_register<int16_t>(ICM20984Register::ACCEL_XOUT_H));
        auto ay = TRY(this->device.read_be_register<int16_t>(ICM20984Register::ACCEL_YOUT_H));
        auto az = TRY(this->device.read_be_register<int16_t>(ICM20984Register::ACCEL_ZOUT_H));

        auto gx = TRY(this->device.read_be_register<int16_t>(ICM20984Register::GYRO_XOUT_H));
        auto gy = TRY(this->device.read_be_register<int16_t>(ICM20984Register::GYRO_YOUT_H));
        auto gz = TRY(this->device.read_be_register<int16_t>(ICM20984Register::GYRO_ZOUT_H));

        return IMUData{
            .ax = static_cast<float>(ax) / ACCEL_SENS,
            .ay = static_cast<float>(ay) / ACCEL_SENS,
            .az = static_cast<float>(az) / ACCEL_SENS,
            .gx = static_cast<float>(gx) / GYRO_SENS,
            .gy = static_cast<float>(gy) / GYRO_SENS,
            .gz = static_cast<float>(gz) / GYRO_SENS,
        };
    }
}