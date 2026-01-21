#include "MLX90395.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

const float gain_sel_table[16] = {
    0.2, 0.25, 0.333, 0.4, 
    0.5, 0.6, 0.75, 1.0,
    0.1, 0.125, 0.1667, 0.2,
    0.25, 0.3, 0.375, 0.5
}

namespace seds {
    enum class MLX90395Register : uint8_t {
        GAIN_SEL = 0x00,
        DATA = 0x80,
    };

    Expected<MLX90395> MLX90395::create(I2CDevice&& device) {
        MLX90395 sensor(std::move(device));

        uint16_t gain_sel = (TRY(sensor.device.read_be_register<uint16_t>(MLX90395Register::GAIN_SEL)) >> 4) & 0xF;
        switch (gain_sel) {
            case 8:   sensor.lsb = 7.14; break;
            case 9:   sensor.lsb = 2.5; break;
            default:
                return make_unique<errors::EspError>(ESP_ERR_INVALID_STATE);
        }

        sensor.gain_sel = gain_sel;
        
        ESP_LOGI("MLX90395", "MLX90395 Magnometer created");

        return sensor;
    } 

    Expected<std::monostate> MLX90395::set_gain(uint16_t gain_sel) {
        if (gain_sel > 15) {
            return make_unique<errors::EspError>(ESP_ERR_INVALID_ARG);
        }

        uint16_t reg_value = TRY(this->device.read_be_register<uint16_t>(MLX90395Register::GAIN_SEL)) 
            & 0xFF0F & (gain_sel << 4);

        TRY(this->device.write_be_register<uint16_t>(MLX90395Register::GAIN_SEL, reg_value));

        this->gain_sel = gain_sel;

        return std::monostate {};
    }

    Expected<MLX90395Data> MLX90395::read_magnetic_field() {
        // first two bytes are unused config
        auto raw_data = TRY(this->device.read_be_register<std::array<int16_t, 4>>(MLX90395Register::DATA));

        MLX90395Data data;
        data.mx = static_cast<float>(raw_data[1]) * this->lsb * gain_sel_table[this->gain_sel];
        data.my = static_cast<float>(raw_data[2]) * this->lsb * gain_sel_table[this->gain_sel];
        data.mz = static_cast<float>(raw_data[3]) * this->lsb * gain_sel_table[this->gain_sel];

        return data;
    }
}