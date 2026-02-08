#pragma once

#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    struct MLX90395Data {
        float mx;
        float my;
        float mz;
    };

    class MLX90395 {
    public:
        static constexpr int16_t default_address = 0x0C;

        [[nodiscard]]
        static Expected<MLX90395> create(I2CDevice&& device);
        
        MLX90395(MLX90395&&) = default;
        MLX90395& operator=(MLX90395&&) = default;
        MLX90395(MLX90395 const&) = delete;
        MLX90395& operator=(MLX90395 const&) = delete;

        // should gain_sel be a public enum instead of an array?
        [[nodiscard]]
        Expected<std::monostate> set_gain(uint16_t gain_sel);

        /// Read the magnetic field measurement from the sensor in microteslas (µT).
        [[nodiscard]]
        Expected<MLX90395Data> read_magnetic_field();

    private:
        explicit MLX90395(I2CDevice&& device) : device(std::move(device)) {};

        I2CDevice device;
        uint16_t gain_sel = 0;
        float lsb = 0.0;
    };
}