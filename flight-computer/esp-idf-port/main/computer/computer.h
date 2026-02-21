#pragma once

#include <expected>

#include "esp_err.h"
#include "errors.h"
#include "esp_log.h"
#include "i2c/BMI323.h"
#include "i2c/BMP581.h"
#include "i2c/high_g_accel.h"
#include "i2c/I2C.h"
#include "i2c/MLX90395.h"
#include "i2c/segment7.h"
#include "i2c/TMP1075.h"
#include "sd.h"
#include "utils.h"

namespace seds {
    using namespace seds::errors;

    class FlightComputer {
    public:
        //BMP581 baro1;
        //BMP581 baro2;
        // fix : SegmentDisplay display;
        BMI323 imu;
        //HighGAccel high_g_accel;
        //MLX90395 mag;
        TMP1075 temp;
        SDCard sd;

        Expected<std::monostate> init(void);
 
        void process(uint32_t times, bool endless);

    private:
        static constexpr char filename[] = MOUNT_POINT"/data.csv";  
    };
}