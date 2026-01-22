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
#include "utils.h"

namespace seds {
    using namespace seds::errors;

    class FlightComputer {
    public:

        // ?
        void process(void);

        BMP581 baro;
        SegmentDisplay display;
        BMI323 imu;
        HighGAccel high_g_accel;
        MLX90395 mag;
        TMP1075 temp;
    }
}