#include "BMP581.h"

#include "driver/i2c_master.h"

const float temp_scale = 1.0f / 65536.0f; // 2^16
const float press_scale = 1.0f / 64.0f; // 2^6

namespace seds {
    enum class BMP581Register : uint8_t {
        CHIP_ID = 0x01,
        TMP_DATA = 0x1D,
        PRESS_DATA = 0x20,
        INT_STATUS = 0x27,
        STATUS = 0x28,
        OSR_CONFIG = 0x36,
        ODR_CONFIG = 0x37,
    };

    BMP581::BMP581(I2CDevice&& device) :
        device(std::move(device)) {
    }

    Expected<BMP581> BMP581::create(I2CDevice&& device) {
        BMP581 bmp581(std::move(device));
        if (!bmp581.is_connected()) {
            return std::make_unique<std::runtime_error>("BMP581 not connected or not responding");
        }

        // enter normal mode
        TRY(bmp581.device.write_be_register<uint8_t>(
            BMP581Register::ODR_CONFIG,
            0x01 // normal mode
        ));

        // try to set config for reading pressure 
        TRY(bmp581.device.write_be_register<uint8_t>(
            BMP581Register::OSR_CONFIG,
            0b01010100 // temp oversampling x1, pressure oversampling x4 (standard resolution), pressure reading on 
        ));

        return bmp581;
    }

    bool BMP581::is_connected() {
        auto const id = this->device.read_be_register<uint8_t>(BMP581Register::CHIP_ID);
        auto const int_status = this->device.read_be_register<uint8_t>(BMP581Register::INT_STATUS);
        auto const status = this->device.read_be_register<uint8_t>(BMP581Register::STATUS);

        // according to the datasheet, valid ID is non-zero
        // valid int_status eqwuals 0x10
        // valid status has bits 1 and 2 set 
        return id.has_value() && int_status.has_value() && status.has_value() 
            && id.value() != 0 && int_status.value() == 0x10 && status.value() & 0x06 == 0x06; 
    }

    Expected<BarometerData> BMP581::read_data() {
        int32_t raw_temp = TRY(this->device.read_le_register<uint32_t>(BMP581Register::TMP_DATA)) & 0xFFFFFF;
        int32_t raw_press = TRY(this->device.read_le_register<uint32_t>(BMP581Register::PRESS_DATA)) & 0xFFFFFF;

        BarometerData data;
        data.baro_temp = static_cast<float>(raw_temp) * temp_scale;
        data.pressure = static_cast<float>(raw_press) * press_scale;

        return data;
    }

}