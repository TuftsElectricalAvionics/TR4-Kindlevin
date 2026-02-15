// I2C Manager Task

#include "i2c.h"

#include <chrono>
#include <atomic>
#include "../utils.h"
#include "i2c/I2C.h"
#include "i2c/TMP1075.h"

using namespace std::chrono_literals;

namespace seds {
    constexpr char const* TAG = "i2c_mgr";

    std::mutex I2CManager::publisher_m;
    std::condition_variable I2CManager::publisher;
    I2CReadings I2CManager::readings;

    I2CManager I2CManager::start() {
        ESP_LOGI(TAG, "Starting I2C manager thread");
        return I2CManager();
    }


    void I2CManager::task_impl() {
        auto i2c = I2C::create();
        ESP_LOGI(TAG, "I2C initialized successfully");

        I2CReadings pending_readings {};
        auto temp_sensor = unwrap(i2c->get_device<TMP1075>());
        ESP_LOGI(TAG, "TMP connected: %d", temp_sensor.is_connected());

        while (true) {
            pending_readings.version += 1;

            if (auto reading = temp_sensor.get()) {
                pending_readings.temp_celsius = reading.value();
            } else {
                ESP_LOGE(TAG, "Temp read failed: %s", reading.error()->what());
                pending_readings.temp_celsius = std::nullopt;
            }

            publish(pending_readings);
            std::this_thread::sleep_for(50ms);
        }
    }

    void I2CManager::publish(I2CReadings const& new_readings) {
        // Update published data
        std::unique_lock lock(publisher_m);
        readings = new_readings;
        lock.unlock();

        // Tell subscribers to check their wait conditions again
        publisher.notify_all();
    }

    I2CReadings I2CManager::wait_for_reading(uint64_t const last_version) {
        std::unique_lock lock(publisher_m);
        // Wait until the version has incremented above `last_version`, indicating new readings.
        publisher.wait(lock, [&]{ return readings.version > last_version; });

        return readings;
    }

}
