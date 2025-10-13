#pragma once
#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    /// TMP1075 Temperature sensor.
    struct TMP1075 {
        static constexpr int16_t default_address = 0x48;

        explicit TMP1075(I2CDevice&& device);

        [[nodiscard]]
        Result<float> read_temperature();

        I2CDevice device;
    };
};