#pragma once
#include <array>
#include <algorithm> 
#include <type_traits>
#include <cstdint>

namespace seds::num {
    template<typename T>
    std::array<uint8_t, sizeof(T)> to_be_bytes(T const number) {
        static_assert(std::is_arithmetic_v<T>);

        auto buffer = std::bit_cast<std::array<uint8_t, sizeof(T)>>(number);

        if constexpr (std::endian::native != std::endian::big) {
            std::ranges::reverse(buffer);
        }

        return buffer;
    }

    template<typename T>
    T from_be_bytes(std::array<uint8_t, sizeof(T)> buffer) {
        static_assert(std::is_arithmetic_v<T>);

        if constexpr (std::endian::native != std::endian::big) {
            std::ranges::reverse(buffer);
        }

        return std::bit_cast<T>(buffer);
    }

    template<typename T>
    std::array<uint8_t, sizeof(T)> to_le_bytes(T const number) {
        static_assert(std::is_arithmetic_v<T>);

        auto buffer = std::bit_cast<std::array<uint8_t, sizeof(T)>>(number);

        if constexpr (std::endian::native != std::endian::little) {
            std::ranges::reverse(buffer);
        }

        return buffer;
    }

    template<typename T>
    T from_le_bytes(std::array<uint8_t, sizeof(T)> buffer) {
        static_assert(std::is_arithmetic_v<T>);

        if constexpr (std::endian::native != std::endian::little) {
            std::ranges::reverse(buffer);
        }

        return std::bit_cast<T>(buffer);
    }
}