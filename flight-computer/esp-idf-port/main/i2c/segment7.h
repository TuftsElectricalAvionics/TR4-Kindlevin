#pragma once

#include "I2C.h"

const uint8_t ALPHA_TABLE[26] = {
    0b01110111,
    0b01111100,
    0b00111001,
    0b01011110,
    0b01111001,
    0b01110001,
    0b00111101,
    0b01110110,
    0b00000110,
    0b00011110,
    0b01111010,
    0b00111000,
    0b00110101,
    0b00110111,
    0b01011100,
    0b01110011,
    0b01100111,
    0b01010000,
    0b01101101,
    0b01111000,
    0b00111110,
    0b00011100,
    0b00101010,
    0b01010101,
    0b01101110,
    0b01011011
};

const uint8_t DIGIT_TABLE[16] = {
    0b00111111,
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00101011,
    0b01111111,
    0b01101111,
    ALPHA_TABLE[0],
    ALPHA_TABLE[1],
    ALPHA_TABLE[2],
    ALPHA_TABLE[3],
    ALPHA_TABLE[4],
    ALPHA_TABLE[5],
};

namespace seds {
    using namespace seds::errors;

    class TCA6507 {
    public:
        static constexpr int16_t default_address = 0x85;

        // I don't think there's any way to fail initilization
        explicit TCA6507(I2CDevice&& device) : device(std::move(device)), msg(std::vector()) {}

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

        [[nodiscard]]
        Expected<std::monostate> set_msg(const std::string msg);

        // TODO: add more high-level display methods
        // We can't do this until we've decided on what pins are mapped to which segments
    private:
        I2CDevice device;
        uint8_t current_segments = 0;
        std::vector<uint8_t> msg; // using vector, but this isn't hot code so it shouldn't matter
        uint32_t msg_offset; 

    }
}