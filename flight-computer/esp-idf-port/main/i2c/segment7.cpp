#include "segment7.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

namespace seds {
    Expected<std::monostate> TCA6507::set_segments(const uint8_t segments) {
        // We're only using fully on or fully off, so we just need to write to register 2

        auto const result = this->device.write_register(0x02, segments);
        if (!result.has_value()) {
            return result.error();
        }

        this->current_segments = segments;
        return std::monostate{};
    }
} 