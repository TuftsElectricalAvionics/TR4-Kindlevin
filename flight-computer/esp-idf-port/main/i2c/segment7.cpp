#include "segment7.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace seds {
    Expected<std::monostate> TCA6507::set_segments(const uint8_t segments) {
        // We're only using fully on or fully off, so we just need to write to register 2
        // TODO NAME REGISTER
        TRY(this->device.write_be_register(0x02, segments));
        this->current_segments = segments;
        return std::monostate {};
    }

    Expected<std::monostate> SegmentDisplay::start_timer(bool restart) {
        if (!restart && this->scroll_timer == nullptr) {
            return std::monostate {};
        }

        if (this->scroll_timer) {
            ESP_TRY(esp_timer_delete(this->scroll_timer));
            this->scroll_timer = nullptr;
        }

        esp_timer_create_args_t args = {
            .callback = [](void* disp) {
                auto ret = static_cast<SegmentDisplay*>(disp)->scroll_msg();
                if (!ret.has_value()) {
                    ESP_LOGE("7segment", "handle timer: %s", ret.error()->what());
                }
            },
            .arg = static_cast<void*>(this),
            .name = "7-segment timer"
        };
        ESP_TRY(esp_timer_create(&args, &this->scroll_timer));
        ESP_TRY(esp_timer_start_periodic(this->scroll_timer, this->scroll_interval_us));

        return std::monostate {};
    }


    Expected<std::monostate> SegmentDisplay::set_msg(const std::string msg) {
        this->msg = std::vector<uint8_t>(msg.begin(), msg.end());
        this->msg_offset = 0;
        if (auto ret = this->start_timer(false); !ret.has_value()) {
            ESP_LOGE("7segment", "timer start: %s", ret.error()->what());
        }
        return std::monostate {};
    }

    Expected<std::monostate> SegmentDisplay::clear_msg() {
        ESP_TRY(esp_timer_stop(this->scroll_timer));
        ESP_TRY(esp_timer_delete(this->scroll_timer));
        this->msg.clear();
        this->msg_offset = 0;
        // Clear both displays
        TRY(this->left_display.set_segments(0x00));
        TRY(this->right_display.set_segments(0x00));
        return std::monostate {};
    }

    Expected<std::monostate> SegmentDisplay::scroll_msg() {
        if (this->msg.empty()) {
            // Nothing to do
            return std::monostate {};
        }

        // Display two characters starting from msg_offset
        uint8_t left_char = this->msg[this->msg_offset];
        uint8_t right_char = ' ';
        if (this->msg_offset + 1 < this->msg.size()) {
            right_char = this->msg[this->msg_offset + 1];
        }

        TRY(this->left_display.set_segments(left_char));
        TRY(this->right_display.set_segments(right_char));

        if (this->msg_offset + 1 >= this->msg.size()) {
            // don't overshoot the beginning of the message
            this->msg_offset = 0;
        } else {
            this->msg_offset = (this->msg_offset + 2) % this->msg.size();
        }

        return std::monostate {};
    }
}
