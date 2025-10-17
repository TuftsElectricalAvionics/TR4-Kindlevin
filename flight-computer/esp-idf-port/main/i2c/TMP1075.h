#pragma once
#include <functional>

#include "I2C.h"

namespace seds {
    using namespace seds::errors;

    /// TMP1075 Temperature sensor.
    class TMP1075 {
    public:
        static constexpr int16_t default_address = 0x48;

        explicit TMP1075(I2CDevice&& device);

        /// Determines how often temperature is sampled. The device's current draw is lower between
        /// samples, when it is in standby mode.
        enum class Rate : uint8_t {
            /// 27.5ms period (default)
            Fastest = 0,
            /// 55ms period
            Fast,
            /// 110ms period
            Normal,
            /// 220ms period
            Slow,
        };

        struct Config {
            /// Controls how fast the temperature sensor is running (default 27.5ms).
            Rate rate = Rate::Fastest;
            /// Sets whether the device is shutdown, making it always in standby mode.
            /// (default false)
            bool is_shutdown = false;
        };

        /// Check whether the device is a working TMP1075 temperature sensor.
        bool is_connected();

        /// Read the last temperature measurement from the sensor in degrees Celsius.
        [[nodiscard]]
        Expected<float> read_temperature();

        /// Run a closure which can read the device's current configuration and update it
        /// as desired.
        ///
        /// Any updates are propagated to the device after the closure finishes running.
        Expected<std::monostate> update_config(std::function<void(Config &)> const& updater) {
            this->current_config = TRY(this->read_config());
            updater(this->current_config);
            return this->write_config(this->current_config, false);
        }

        /// Instruct the device to update its reading even though it is in shutdown mode.
        Expected<std::monostate> take_sample() {
            return this->write_config(this->current_config, true);
        }

    private:
        Expected<std::monostate> write_config(Config const& config, bool do_oneshot);
        Expected<Config> read_config();

        I2CDevice device;
        Config current_config;
    };
};