#pragma once
#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    struct HighGAccelData {
        float h_ax;
        float h_ay;
        float h_az;
    };

    /// ADXL375BCCZ high-g accelerometer
    class HighGAccel {
    public:
        static constexpr int16_t default_address = 0x53;

        /// Should this function initialize reading?
        [[nodiscard]]
        static Expected<HighGAccel> create(I2CDevice&& device);

        bool is_connected();

        [[nodiscard]]
        Expected<HighGAccelData> read_acceleration();

    private:
        explicit HighGAccel(I2CDevice&& device);

        I2CDevice device;
    };
}