#include "BMI323.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

// Note: The BMI323 IMU stores 16 bits in each register
// This is unlike other devices we use, where 16-bit values are stored in two sequential register
// Additionally, when using I2C, the BMI323 inserts 2 dummy bytes before the data
// This means that, in general, our strategy will be to read a uint32_t, then truncate to a uint16_t
// TODO: Check this!

namespace seds {
    enum class BMI323Register : uint8_t {
        CHIP_ID = 0x00,
        ACC_DATA_X = 0X03,
        ACC_DATA_Y = 0X04,
        ACC_DATA_Z = 0X05,
        GYR_DATA_X = 0X06,
        GYR_DATA_Y = 0X07,
        GYR_DATA_Z = 0X08,
        ACC_CFG = 0x20,
        GYR_CFG = 0x21,
    };

    // Both of these are exactly the values given, scaled up by 1000 for LSB/g
    // However, the actual values are probably powers of two, so we could guess what they are and get slightly better precision
    // For now, I'll use what I've found in the data sheet 

    const float accel_sens[4] = {16380.0, 8190.0, 4010.0, 2050.0}; // LSB/g for ranges 2g, 4g, 8g, 16g
    const float gyro_sens[5] = {262.144, 131.072, 65.536, 32.768, 16.4}; // LSB/°/s for ranges 125, 250, 500, 1000, 2000 dps

    BMI323::BMI323(I2CDevice&& device) : device(std::move(device)) {}

    Expected<BMI323> BMI323::create(I2CDevice&& device) {
        auto imu = BMI323(std::move(device));

        // configuration
        // should we make this changeable on the fly?
        uint16_t acc_range = AccelRange::_2G; // +/- 8g
        uint16_t acc_hz = SensorHz::_100; // 100Hz
        uint16_t acc_mode = 0x4; // normal mode

        uint16_t acc_cfg = (acc_mode << 12) | (acc_range << 4) | acc_hz;

        TRY(
            imu.device.write_le_register<uint16_t>(
                BMI323Register::ACC_CFG,
                acc_cfg
            )
        );

        uint16_t gyr_range = GyroRange::_500DPS; // +/- 500dps
        uint16_t gyr_hz = SensorHz::_100; // 100Hz
        uint16_t gyr_mode = 0x4; // normal mode

        uint16_t gyr_cfg = (gyr_mode << 12) | (gyr_range << 4) | gyr_hz;

        TRY(
            imu.device.write_le_register<uint16_t>(
                BMI323Register::GYR_CFG,
                gyr_cfg
            )
        );

        imu.accel_range = AccelRange::_8G;
        imu.gyro_range = GyroRange::_500DPS;
        imu.sensor_hz = SensorHz::_100;

        ESP_LOGI("BMI323", "IMU created");

        return imu;
    }

    bool BMI323::is_connected() {
        constexpr uint16_t device_id = 0x0043; // BMI323 ID

        auto result = (uint16_t)(imu.device.read_le_register<uint32_t>(BMI323Register::ACCEL_X_LSB));
        return result.has_value() && result.value() == device_id;
    }   

    Expected<std::monostate> BMI323::set_accel_range(AccelRange range) {
        uint16_t acc_range = static_cast<uint16_t>(range);
        uint16_t acc_hz = this->sensor_hz; // 100Hz
        uint16_t acc_mode = 0x4; // normal mode

        uint16_t acc_cfg = (acc_mode << 12) | (acc_range << 4) | acc_hz;

        TRY(
            this->device.write_le_register<uint16_t>(
                BMI323Register::ACC_CFG,
                acc_cfg
            )
        );

        this->accel_range = range;

        return std::monostate {};
    }

    Expected<std::monostate> BMI323::set_gyro_range(GyroRange range) {
        uint16_t gyr_range = static_cast<uint16_t>(range);
        uint16_t gyr_hz = this->sensor_hz; // 100Hz
        uint16_t gyr_mode = 0x4; // normal mode

        uint16_t gyr_cfg = (gyr_mode << 12) | (gyr_range << 4) | gyr_hz;

        TRY(
            this->device.write_le_register<uint16_t>(
                BMI323Register::GYR_CFG,
                gyr_cfg
            )
        );

        this->gyro_range = range;

        return std::monostate {};
    }

    Expected<IMUData> BMI323::read_imu() {
        // Read accelerometer data
        auto ax_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_X)));
        auto ay_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Y)));
        auto az_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Z)));

        // Read gyroscope data
        auto gx_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_X)));
        auto gy_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Y)));
        auto gz_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Z)));

        IMUData data;
        data.ax = static_cast<float>(ax_raw) / accel_sens[this->accel_range];
        data.ay = static_cast<float>(ay_raw) / accel_sens[this->accel_range];
        data.az = static_cast<float>(az_raw) / accel_sens[this->accel_range];
        data.gx = static_cast<float>(gx_raw) / gyro_sens[this->gyro_range];
        data.gy = static_cast<float>(gy_raw) / gyro_sens[this->gyro_range];
        data.gz = static_cast<float>(gz_raw) / gyro_sens[this->gyro_range];

        return data;
    }
}