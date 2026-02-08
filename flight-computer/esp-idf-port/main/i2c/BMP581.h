#pragma once

#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    struct BarometerData {
        float baro_temp;
        float pressure;
    };

    class BMP581 {
    public:
        // No default address since there are two, so we should specify each
        static constexpr int16_t address_1 = 0x46;
        static constexpr int16_t address_2 = 0x47;

        [[nodiscard]]
        Expected<BMP581> create(I2CDevice&& device);

        // No copies allowed since we hold unique state
        BMP581(BMP581&&) = default;
        BMP581& operator=(BMP581&&) = default;
        BMP581(BMP581 const&) = delete;
        BMP581& operator=(BMP581 const&) = delete;

        /// Check whether the device is a working BMP581 barometer.
        bool is_connected();

        /// Read the last temperature and pressure measurement from the sensor.
        [[nodiscard]]
        Expected<BarometerData> read_data();

    private:
        explicit BMP581(I2CDevice&& device);

        I2CDevice device;
    };
}