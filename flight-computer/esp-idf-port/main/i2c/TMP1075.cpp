#include "TMP1075.h"
#include <span>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace seds {
    enum class TMP1075Register : uint8_t {
        TEMP = 0x00,
        CFGR = 0x01,
        LLIM = 0x02,
        HLIM = 0x03,
        DIEID = 0x0F,
    };

    TMP1075::TMP1075(I2CDevice&& device) :
        device(std::move(device)) {
        ESP_LOGI("TMP1075", "Temp sensor created");
    }

    bool TMP1075::is_connected() {
        constexpr uint16_t device_id = 0x7500; // 75 for TMP1075

        auto const reg = this->device.read_be_register<uint16_t>(TMP1075Register::DIEID);
        if (!reg.has_value()) {
            return false;
        }

        return reg.value() == device_id;
    }

    Expected<float> TMP1075::read_temperature() {
        // https://www.ti.com/lit/an/sbaa588a/sbaa588a.pdf?ts=1760629136511

        // Bits 15-4 contain signed big-endian temperature, 3-0 are unused
        auto temp_raw = TRY(this->device.read_be_register<int16_t>(TMP1075Register::TEMP));

        // Move the entire int down into the first few bytes. (Sign extension is automatic.)
        temp_raw >>= 4;

        // Apply resolution scaling factor specified in data sheet
        return static_cast<float>(temp_raw) * 0.0625f;
    }

    constexpr uint8_t ONESHOT_OFFSET = 15;
    constexpr uint8_t RATE_OFFSET = 13;
    constexpr uint8_t SHUTDOWN_OFFSET = 8;

    Expected<std::monostate> TMP1075::write_config(Config const& config, bool const do_oneshot) {
        uint16_t config_data = 0;

        config_data |= do_oneshot << ONESHOT_OFFSET;
        config_data |= static_cast<uint8_t>(config.rate) << RATE_OFFSET;
        config_data |= config.is_shutdown << SHUTDOWN_OFFSET;

        return this->device.write_be_register(TMP1075Register::CFGR, config_data);
    }

    Expected<TMP1075::Config> TMP1075::read_config() {
        auto const reg = TRY(this->device.read_be_register<uint16_t>(TMP1075Register::CFGR));

        return Config {
            .rate = static_cast<Rate>(reg >> RATE_OFFSET & 0b11),
            .is_shutdown = (reg >> SHUTDOWN_OFFSET & 0b1) == 1,
        };
    }
}
