#pragma once

#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    class TCA6507 {
    public:
        static constexpr int16_t default_address = 0x85;

        // I don't think there's any way to fail initilization
        explicit TCA6507(I2CDevice&& device) : device(std::move(device)) {}

        TCA6507(TCA6507&&) = default;
        TCA6507& operator=(TCA6507&&) = default;
        TCA6507(TCA6507 const&) = delete;
        TCA6507& operator=(TCA6507 const&) = delete;

        /// Set the segments to display.
        ///
        /// Each byte in `segments` corresponds to one digit, with bit 0 controlling segment A,
        /// bit 1 controlling segment B, and so on up to bit 6 controlling segment G. 
        /// There isn't any way to control the decimal point segments with the driver we're using.
        [[nodiscard]]
        Expected<std::monostate> set_segments(const uint8_t segments);    

        // TODO: add more high-level display methods
        // We can't do this until we've decided on what pins are mapped to which segments
    private:
        I2CDevice device;
        uint8_t current_segments = 0;
    }
}