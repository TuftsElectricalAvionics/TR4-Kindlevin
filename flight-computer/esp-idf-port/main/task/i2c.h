// Interface for I2C Manager

#pragma once

#include <optional>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "esp_log.h"

namespace seds {
    struct I2CReadings {
        I2CReadings() = default;
        I2CReadings(I2CReadings const&) = default;

        uint64_t version = 0;
        // TODO: Make seds::Expected<T> cheaper to copy so it can be used here.
        std::optional<float> temp_celsius;
    };

    class I2CManager: std::thread {
        [[noreturn]]
        static void task_impl();
        explicit I2CManager() : std::thread(task_impl) {}

        static std::mutex publisher_m;
        static std::condition_variable publisher;
        static I2CReadings readings;

        /// Makes new data available for the subscribed threads.
        static void publish(I2CReadings const& new_readings);

    public:
        /// Starts the I2C manager so that calls to latest_reading/wait_for_reading begin to update.
        static I2CManager start();

        /// Returns the latest measurements from the I2C manager's sensors.
        static I2CReadings get_latest_reading() {
            std::lock_guard lock(publisher_m);
            return readings;
        }

        /// Wait until there is a new reading (i.e. one newer than `last_version`), then return it.
        ///
        /// The parameter `last_version` must be the latest version received from an I2C manager's
        /// readings, or 0 if none has been received.
        static I2CReadings wait_for_reading(uint64_t last_version);
    };

    [[noreturn]]
    void i2c_manager_task_impl(void* args);
}
