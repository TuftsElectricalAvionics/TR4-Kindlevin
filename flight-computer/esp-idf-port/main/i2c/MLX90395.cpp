#include "MLX90395.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

const float gain_sel_table[16] = {
    0.2, 0.25, 0.333, 0.4, 
    0.5, 0.6, 0.75, 1.0,
    0.1, 0.125, 0.1667, 0.2,
    0.25, 0.3, 0.375, 0.5
};

namespace seds {
    enum class MLX90395Register : uint8_t {
        GAIN_SEL = 0x00,
        X_DATA = 0x82,
        Y_DATA = 0x84,
        Z_DATA = 0x86
    };

    Expected<MLX90395> MLX90395::create(I2CDevice&& device) {
        MLX90395 sensor(std::move(device));
        uint16_t gain_sel = (TRY(sensor.device.read_be_register<uint16_t>(MLX90395Register::GAIN_SEL)) >> 4) & 0xF;
        switch (gain_sel) {
            case 8:   sensor.lsb = 7.14; break;
            case 9:   sensor.lsb = 2.5; break;
            default:
                return std::unexpected(std::make_unique<EspError>(ESP_ERR_INVALID_STATE));
        }

        sensor.gain_sel = gain_sel;
        
        ESP_LOGI("MLX90395", "MLX90395 Magnometer created");
        return sensor;
    } 

    Expected<std::monostate> MLX90395::set_gain(uint16_t gain_sel) {
        if (gain_sel > 15) {
            return std::unexpected(std::make_unique<EspError>(ESP_ERR_INVALID_ARG));
        }

        uint16_t reg_value = TRY(this->device.read_be_register<uint16_t>(MLX90395Register::GAIN_SEL)) 
            & 0xFF0F & (gain_sel << 4);

        TRY(this->device.write_be_register<uint16_t>(MLX90395Register::GAIN_SEL, reg_value));

        this->gain_sel = gain_sel;

        return std::monostate {};
    }

    Expected<MLX90395Data> MLX90395::read_magnetic_field() {
        
        uint16_t x_data = TRY(this->device.read_be_register<uint16_t>(MLX90395Register::X_DATA));
        uint16_t y_data = TRY(this->device.read_be_register<uint16_t>(MLX90395Register::Y_DATA));
        uint16_t z_data = TRY(this->device.read_be_register<uint16_t>(MLX90395Register::Z_DATA));

        MLX90395Data data;
        data.mx = static_cast<float>(x_data) * this->lsb * gain_sel_table[this->gain_sel];
        data.my = static_cast<float>(y_data) * this->lsb * gain_sel_table[this->gain_sel];
        data.mz = static_cast<float>(z_data) * this->lsb * gain_sel_table[this->gain_sel];

        return data;
    }
}