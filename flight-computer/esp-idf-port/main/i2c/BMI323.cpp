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

    BMI323::BMI323(I2CDevice&& device) : device(std::move(device)) {}

    Result<BMI323> BMI323::create(I2CDevice&& device) {
        auto imu = BMI323(std::move(device));

        // configuration
        // should we make this changeable on the fly?
        uint16_t acc_range = 0x2; // +/- 8g
        uint16_t acc_hz = 0x8; // 100Hz
        uint16_t acc_mode = 0x4; // normal mode

        uint16_t acc_cfg = (acc_mode << 12) | (acc_range << 4) | acc_hz;

        TRY(
            imu.device.write_le_register<uint16_t>(
                BMI323Register::ACC_CFG,
                acc_cfg
            )
        );

        uint16_t gyr_range = 0x2; // +/- 500dps
        uint16_t gyr_hz = 0x8; // 100Hz
        uint16_t gyr_mode = 0x4; // normal mode

        uint16_t gyr_cfg = (gyr_mode << 12) | (gyr_range << 4) | gyr_hz;

        TRY(
            imu.device.write_le_register<uint16_t>(
                BMI323Register::GYR_CFG,
                gyr_cfg
            )
        );

        imu.accel_sens = 4100.; // LSB/g (actually 4.1 LSB/mg)
        imu.gyro_sens = 65.536;    // LSB/°/s

        ESP_LOGI("BMI323", "IMU created");

        return imu;
    }

    bool BMI323::is_connected() {
        constexpr uint16_t device_id = 0x0043; // BMI323 ID

        auto result = (uint16_t)(imu.device.read_le_register<uint32_t>(BMI323Register::ACCEL_X_LSB));
        return result.has_value() && result.value() == device_id;
    }   

    Result<IMUData> BMI323::read_imu() {
        // Read accelerometer data
        auto ax_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_X)));
        auto ay_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Y)));
        auto az_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Z)));

        // Read gyroscope data
        auto gx_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_X)));
        auto gy_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Y)));
        auto gz_raw = TRY((uint16_t)(this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Z)));

        IMUData data;
        data.ax = static_cast<float>(ax_raw) / this->accel_sens;
        data.ay = static_cast<float>(ay_raw) / this->accel_sens;
        data.az = static_cast<float>(az_raw) / this->accel_sens;
        data.gx = static_cast<float>(gx_raw) / this->gyro_sens;
        data.gy = static_cast<float>(gy_raw) / this->gyro_sens;
        data.gz = static_cast<float>(gz_raw) / this->gyro_sens;

        return data;
    }
}