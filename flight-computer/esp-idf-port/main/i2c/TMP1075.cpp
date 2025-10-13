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

    TMP1075::TMP1075(I2CDevice&& device) : device(std::move(device)) {
        ESP_LOGI("TMP1075", "Temp sensor created");
    }

    Result<float> TMP1075::read_temperature() {
        return TRY(this->device.read_register<float>(TMP1075Register::TEMP));
    }
}
