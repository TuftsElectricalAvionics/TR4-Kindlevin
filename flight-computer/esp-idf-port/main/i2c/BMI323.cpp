#include "BMI323.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"

// Note: The BMI323 IMU stores 16 bits in each register
// This is unlike other devices we use, where 16-bit values are stored in two sequential register
// Additionally, when using I2C, the BMI323 inserts 2 dummy bytes before the data
// This means that, in general, our strategy will be to read a uint32_t, then truncate to a uint16_t
// TODO: Check this!

namespace seds {
    enum class BMI323Register : uint8_t {
        CHIP_ID = 0x00,
        ERR_REG = 0x01,
        STATUS = 0x02,
        ACC_DATA_X = 0X03,
        ACC_DATA_Y = 0X04,
        ACC_DATA_Z = 0X05,
        GYR_DATA_X = 0X06,
        GYR_DATA_Y = 0X07,
        GYR_DATA_Z = 0X08,
        ACC_CFG = 0x20,
        GYR_CFG = 0x21,
        CMD = 0x7E,
    };

    // Both of these are exactly the values given, scaled up by 1000 for LSB/g
    // However, the actual values are probably powers of two, so we could guess what they are and get slightly better precision
    // For now, I'll use what I've found in the data sheet 

    const std::array<float, 4> accel_divisors {16380.0, 8190.0, 4010.0, 2050.0}; // LSB/g for ranges 2g, 4g, 8g, 16g
    const std::array<float, 5> gyro_divisors {262.144, 131.072, 65.536, 32.768, 16.4}; // LSB/°/s for ranges 125, 250, 500, 1000, 2000 dps

    BMI323::BMI323(I2CDevice&& device) : device(std::move(device)) {}

    Expected<std::monostate> BMI323::write_accel_config(AccelRange range, SensorHz hz) {
        uint16_t acc_mode = 0x4; // normal mode
        uint16_t acc_cfg = (acc_mode << 12) | (static_cast<uint16_t>(range) << 4) | static_cast<uint16_t>(hz);
        acc_cfg &= 0x707F;

        TRY(
            this->device.write_le_register<uint16_t>(
                BMI323Register::ACC_CFG,
                acc_cfg
            )
        );

        return std::monostate {};
    }

    Expected<std::monostate> BMI323::write_gyro_config(GyroRange range, SensorHz hz) {
        uint16_t gyr_mode = 0x4; // normal mode
        uint16_t gyr_cfg = (gyr_mode << 12) | (static_cast<uint16_t>(range) << 4) | static_cast<uint16_t>(hz);
        gyr_cfg &= 0x707F;

        TRY(
            this->device.write_le_register<uint16_t>(
                BMI323Register::GYR_CFG,
                gyr_cfg
            )
        );

        return std::monostate {};
    }

    Expected<BMI323> BMI323::create(I2CDevice&& device) {
        auto imu = BMI323(std::move(device));

        // Set configuration after write in case of failure
        // This is why the writing functions take parameters rather than using the member variables directly
        imu.accel_range = AccelRange::_8G;
        imu.gyro_range = GyroRange::_500DPS;
        imu.sensor_hz = SensorHz::_100;

        // force soft reset
        TRY(imu.device.write_le_register<uint16_t>(BMI323Register::CMD, 0xDEAF));
        // pause
        vTaskDelay(pdMS_TO_TICKS(10));

        uint16_t acc_cfg = TRY(imu.device.read_le_register<uint32_t>(BMI323Register::ACC_CFG)) >> 16;
        ESP_LOGI("BMI323", "acc cfg: %x", acc_cfg);

        TRY(imu.write_accel_config(imu.accel_range, imu.sensor_hz));
        TRY(imu.write_gyro_config(imu.gyro_range, imu.sensor_hz));
        
        ESP_LOGI("BMI323", "IMU created");

        return imu;
    }

    bool BMI323::is_connected() {
        constexpr uint16_t dev_id = 0x0043;
        auto result = this->device.read_le_register<uint32_t>(BMI323Register::CHIP_ID);
        if (result.has_value()) {
            ESP_LOGE("BMI323", "dev id: %" PRIx32, (uint32_t)result.value());
        } else { ESP_LOGE("BMI323", "dev id read failed"); }
        if (!result.has_value() || (uint16_t)((result.value() >> 16) & 0xFF) != dev_id) {
            ESP_LOGE("BMI323", "Chip ID read failed or incorrect");
            return false;
        }

        result = this->device.read_le_register<uint32_t>(BMI323Register::ERR_REG);
        if (result.has_value()) {
            ESP_LOGE("BMI323", "err reg: %" PRIx32, (uint32_t)result.value());
        } else { ESP_LOGE("BMI323", "err reg read failed"); }
        if (!result.has_value() || (uint16_t)(result.value() >> 16) != 0) {
            ESP_LOGE("BMI323", "Err reg read failed or incorrect");
            return false;
        }

        // reads inconsistent values, should remove in the future
        // TODO
        /*
        result = this->device.read_le_register<uint32_t>(BMI323Register::STATUS);
        if (result.has_value()) {
            ESP_LOGE("BMI323", "status: %" PRIx32, (uint32_t)result.value());
        } else { ESP_LOGE("BMI323", "status read failed"); }
        if (!result.has_value() || (uint16_t)((result.value() >> 16) & 0xE1) != 1) {
            ESP_LOGE("BMI323", "status read failed or incorrect");
            return false;
        }
        */

        return true;
    }

    Expected<std::monostate> BMI323::set_accel_range(AccelRange range) {
        TRY(this->write_accel_config(range, this->sensor_hz));

        this->accel_range = range;

        return std::monostate {};
    }

    Expected<std::monostate> BMI323::set_gyro_range(GyroRange range) {
        TRY(this->write_gyro_config(range, this->sensor_hz));

        this->gyro_range = range;

        return std::monostate {};
    }

    Expected<std::monostate> BMI323::set_sensor_hz(SensorHz hz) {
        TRY(this->write_accel_config(this->accel_range, hz));
        TRY(this->write_gyro_config(this->gyro_range, hz));

        this->sensor_hz = hz;

        return std::monostate {};
    }

    Expected<IMUData> BMI323::read_imu() {
        // The device sends 32-bit LE numbers, but the upper 16 bits are always zeroed out
        // according to page 208 of the datasheet.

        // Read accelerometer data
        int16_t ax_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_X))) >> 16;
        int16_t ay_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Y))) >> 16;
        int16_t az_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::ACC_DATA_Z))) >> 16;

        // Read gyroscope data
        int16_t gx_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_X))) >> 16;
        int16_t gy_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Y))) >> 16;
        int16_t gz_raw = TRY((this->device.read_le_register<uint32_t>(BMI323Register::GYR_DATA_Z))) >> 16;

        auto accel_range = static_cast<size_t>(this->accel_range);
        auto gyro_range = static_cast<size_t>(this->gyro_range);

        ESP_LOGI("BMI323", "ax raw: %d, ay raw: %d, az raw: %d, accel range: %f",
            ax_raw, ay_raw, az_raw, accel_divisors[accel_range]);

        IMUData data = {
            .ax = static_cast<float>(ax_raw) / accel_divisors[accel_range],
            .ay = static_cast<float>(ay_raw) / accel_divisors[accel_range],
            .az = static_cast<float>(az_raw) / accel_divisors[accel_range],
            .gx = static_cast<float>(gx_raw) / gyro_divisors[gyro_range],
            .gy = static_cast<float>(gy_raw) / gyro_divisors[gyro_range],
            .gz = static_cast<float>(gz_raw) / gyro_divisors[gyro_range],
        };

        return data;
    }
}