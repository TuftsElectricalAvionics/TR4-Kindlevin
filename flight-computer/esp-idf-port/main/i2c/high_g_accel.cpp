#include "high_g_accel.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

/// TODO: Config?
namespace seds {
    enum class ADXL375Register : uint8_t {
        DEVID = 0x00,
        POWER_CTL = 0x2D,
        DATA_FORMAT = 0x31,
        DATAX0 = 0x32,
        DATAY0 = 0x34,
        DATAZ0 = 0x36,
    };

    constexpr float accel_sense = 20.5; 

    HighGAccel::HighGAccel(I2CDevice&& device) : device(std::move(device)) {}

    Expected<HighGAccel> HighGAccel::create(I2CDevice&& device)  {
        auto accel = HighGAccel(std::move(device));

        /// Initialize the accelerometer: Full resolution, ±200g
        TRY(
            accel.device.write_be_register<uint8_t>(
                ADXL375Register::DATA_FORMAT,
                0x08 // right justified
            )
        );

        /// Set measure bit to start measurements
        TRY(
            accel.device.write_be_register<uint8_t>(
                ADXL375Register::POWER_CTL,
                0x08
            )
        );

        ESP_LOGI("HighGAccel", "High-g accelerometer created");

        return accel;
    }

    bool HighGAccel::is_connected() {
        constexpr uint16_t device_id = 0xE5; // 75 for ADXL345

        auto result = this->device.read_be_register<uint8_t>(ADXL375Register::DEVID);
        return result.has_value() && result.value() == device_id;
    }

    
    Expected<HighGAccelData> HighGAccel::read_acceleration() {
        // FIXME: read all sensors at once
        auto raw_data = TRY(this->device.read_le_register<uint64_t>(ADXL375Register::DATAX0)) & 0xFFFFFFFFFFFF;

        auto x = (int16_t)raw_data;
        auto y = (int16_t)(raw_data >> 16);
        auto z = (int16_t)(raw_data >> 32);

        HighGAccelData data;
        data.h_ax = static_cast<float>(x) / accel_sense;
        data.h_ay = static_cast<float>(y) / accel_sense;
        data.h_az = static_cast<float>(z) / accel_sense;

        return data;
    }
}