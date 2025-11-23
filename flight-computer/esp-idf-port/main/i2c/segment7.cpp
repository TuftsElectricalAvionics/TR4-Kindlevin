#include "segment7.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
<<<<<<< HEAD
=======
#include "esp_timer.h"
>>>>>>> 361a739890d5aecbced5944435a264fc9769edc4

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
<<<<<<< HEAD
=======

    Expected<std::monostate> TCA6507::set_msg(const std::string msg) {
        auto args = { .callback = , .arg = , .name = "7-segment timer",  }
    }
>>>>>>> 361a739890d5aecbced5944435a264fc9769edc4
} 