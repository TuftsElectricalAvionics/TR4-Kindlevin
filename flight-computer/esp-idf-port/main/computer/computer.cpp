#include "computer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>

static const char *TAG = "computer";

namespace seds {

const gpio_num_t MAIN_CONT = GPIO_NUM_18;
const gpio_num_t DROGUE_CONT = GPIO_NUM_19;
const gpio_num_t DROGUE_CHUTE = GPIO_NUM_22;
const gpio_num_t MAIN_CHUTE = GPIO_NUM_23;

Expected<std::monostate> FlightComputer::init() {
    ESP_TRY(gpio_set_direction(DROGUE_CONT, GPIO_MODE_INPUT));
    ESP_TRY(gpio_set_direction(MAIN_CONT, GPIO_MODE_INPUT));
    ESP_TRY(gpio_set_direction(DROGUE_CHUTE, GPIO_MODE_OUTPUT));
    ESP_TRY(gpio_set_direction(MAIN_CHUTE, GPIO_MODE_OUTPUT));

    char data[] = "timestamp, accel x, accel y, accel z, degrees x, degrees y, degrees z, baro 1 temp, baro 1 pressure, baro 2 temp, baro 2 pressure, high g accel x, high g accel y, high g accel z, temp\n";
    // test different filenames
    bool broke = false;
    struct stat st;
    for (int i = 0; i < 1000; i++) {
        // should write SD functions for this
        // TODO
        // also improve interface so we dont have to do what we do in process()
        snprintf(this->filename, FlightComputer::buf_len, "%s/data%d.csv", MOUNT_POINT, i);
        ESP_LOGI("computer", "filename: %s, res: %d", this->filename, stat(this->filename, &st));
        if (stat(this->filename, &st) == -1) {
            // doesn't exist, we go with it
            broke = true;
            break;
        }
    } 
    
    if (!broke) {
        strcpy(this->filename, MOUNT_POINT"/data.csv");
    }
    ESP_LOGI("computer", "filename: %s", this->filename);

    return this->sd.create_file(this->filename, (uint8_t *)data, sizeof(data)-1); // subtract one
}

// same NOAA pressure-altitude calculation the rrc3 does
float FlightComputer::pressure_to_altitude(float pressure) {
    float pres_mb = pressure / 100;
    float h_alt = 145366.45 * (1 - powf(pres_mb / 1013.25, 0.190284));
    return h_alt;
}

const int64_t APOGEE_WINDOW = 500; // apogee detection window, ~ ms the system will wait before deploy
const bool BARO_AGREE_DROGUE = true; // do the barometers have to agree we've reached apogee before deploying?
const bool BARO_AGREE_MAIN = false; // do the barometers have to agree we're below main altitude?

const float MAIN_ALTITUDE = 500; // feet

const bool BACKUP = false;
const int64_t BACKUP_DELAY = 1000; // ms

const size_t LOOPS_BEFORE_FLUSH = 100;
char buffer[LOOPS_BEFORE_FLUSH * (40 + 14 * 20 + 13 + 1 + 1)];

void FlightComputer::process(uint32_t times, bool endless) {
    FILE *data_file = fopen(this->filename, "a");

    struct timeval tv_now;
    memset(buffer, 'X', sizeof(buffer));

    bool drogue_triggered = false;
    bool main_triggered = false;
    float min_baro1_pres = INFINITY;
    int64_t baro1_timestamp = 0;
    float min_baro2_pres = INFINITY;
    int64_t baro2_timestamp = 0;

    int64_t drogue_backup_trigger_timestamp = 0;
    int64_t main_backup_trigger_timestamp = 0;

    size_t idx = 0;
    for (int i = 0; i < times || endless; i++) {
        gettimeofday(&tv_now, NULL);
        int64_t time_ms = (int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000L;
        IMUData imu_data = {.ax = 0, .ay = 0, .az = 0, .gx = 0, .gy = 0, .gz = 0};
        BarometerData baro_1_data = { .baro_temp = 0, .pressure = 0 };
        BarometerData baro_2_data = { .baro_temp = 0, .pressure = 0 };
        HighGAccelData high_g_data = { .h_ax = 0, .h_ay = 0, .h_az = 0 };
        float tmp = 0.0;

        auto imu_data_try = this->imu.read_imu();
        if (imu_data_try.has_value()) {
            imu_data = imu_data_try.value();
        } else {
            ESP_LOGE(TAG, "imu data read failed");
        }

        auto baro_1_data_try = this->baro1.read_data();
        if (baro_1_data_try.has_value()) {
            baro_1_data = baro_1_data_try.value();
            if (min_baro1_pres >= baro_1_data.pressure) {
                min_baro1_pres = baro_1_data.pressure;
                baro1_timestamp = time_ms;
            }
        } else {
            ESP_LOGE(TAG, "baro 1 data read failed");
        }

        auto baro_2_data_try = this->baro2.read_data();
        if (baro_2_data_try.has_value()) {
            baro_2_data = baro_2_data_try.value();
            if (min_baro2_pres >= baro_2_data.pressure) {
                min_baro2_pres = baro_2_data.pressure;
                baro2_timestamp = time_ms;
            }
        } else {
            ESP_LOGE(TAG, "baro 2 data read failed");
        }

        auto high_g_data_try = this->high_g_accel.read_acceleration();
        if (high_g_data_try.has_value()) {
            high_g_data = high_g_data_try.value();
        } else {
            ESP_LOGE(TAG, "high g data read failed");
        }

        auto tmp_try = this->temp.read_temperature();
        if (tmp_try.has_value()) {
            tmp = tmp_try.value();
        } else {
            ESP_LOGE(TAG, "temp data read failed");
        } 

        // barometer data processing
        // don't mix data across barometers to mitigate barometer errors
        bool baro1_drogue_trigger_yes = (time_ms - baro1_timestamp >= APOGEE_WINDOW) && (min_baro1_pres < baro_1_data.pressure); 
        bool baro2_drogue_trigger_yes = (time_ms - baro2_timestamp >= APOGEE_WINDOW) && (min_baro2_pres < baro_2_data.pressure); 

        bool drogue_trigger = ((BARO_AGREE_DROGUE && baro1_drogue_trigger_yes && baro2_drogue_trigger_yes) 
                || (!BARO_AGREE_DROGUE && (baro1_drogue_trigger_yes || baro2_drogue_trigger_yes))
            ) && !drogue_triggered;

        if (drogue_trigger) {
            if (!BACKUP) {
                esp_err_t err = gpio_set_level(DROGUE_CHUTE, 1);
            
                if (err == ESP_OK && gpio_get_level(DROGUE_CONT) == 0) {
                    drogue_triggered = true;
                }
            } else {
                drogue_backup_trigger_timestamp = time_ms;
            }
        }

        if (drogue_triggered && !main_triggered) {
            bool baro1_main_trigger_yes = FlightComputer::pressure_to_altitude(baro_1_data.pressure) < MAIN_ALTITUDE;
            bool baro2_main_trigger_yes = FlightComputer::pressure_to_altitude(baro_2_data.pressure) < MAIN_ALTITUDE;
            bool main_trigger = (BARO_AGREE_DROGUE && baro1_main_trigger_yes && baro2_main_trigger_yes) 
                || (!BARO_AGREE_MAIN && (baro1_main_trigger_yes || baro2_main_trigger_yes));

            if (main_trigger) {
                if (!BACKUP) {
                    esp_err_t err = gpio_set_level(MAIN_CHUTE, 1);
            
                    if (err == ESP_OK && gpio_get_level(MAIN_CONT) == 0) {
                        main_triggered = true;
                    }
                } else {
                    main_backup_trigger_timestamp = time_ms;
                }
            }
        }

        if (BACKUP) {
            if (!drogue_triggered && drogue_backup_trigger_timestamp != 0 && time_ms - drogue_backup_trigger_timestamp > BACKUP_DELAY) {
                // duplicated code, refactor!
                esp_err_t err = gpio_set_level(DROGUE_CHUTE, 1);
            
                if (err == ESP_OK && gpio_get_level(DROGUE_CONT) == 0) {
                    drogue_triggered = true;
                }
            }

            else if (!main_triggered && main_backup_trigger_timestamp != 0 && time_ms - main_backup_trigger_timestamp > BACKUP_DELAY) {
                // duplicated code, refactor!
                esp_err_t err = gpio_set_level(MAIN_CHUTE, 1);
            
                if (err == ESP_OK && gpio_get_level(MAIN_CONT) == 0) {
                    main_triggered = true;
                }
            }
        }

       
        idx += snprintf(&buffer[idx], 40 + 20 * 14 + 14, "%lld,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
            time_ms,
            imu_data.ax, imu_data.ay, imu_data.az, imu_data.gx, imu_data.gy, imu_data.gz,
            baro_1_data.baro_temp, baro_1_data.pressure, baro_2_data.baro_temp, baro_2_data.pressure,
            high_g_data.h_ax, high_g_data.h_ay, high_g_data.h_az, tmp
        );
        
        // speed this up!s
        if ((i % LOOPS_BEFORE_FLUSH) == LOOPS_BEFORE_FLUSH - 1) {
            ESP_LOGI(TAG, "Flushing (not really)");
            errno = 0;
            //fwrite((void *)buffer, 1, idx, data_file);
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }
            errno = 0;
            //fflush(data_file);
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }
            errno = 0;
            //fsync(fileno(data_file));
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }
            
            //auto append_res = sd.append_file(FlightComputer::filename, (uint8_t *)buffer, idx);
            //if (!append_res.has_value()) {
            //    ESP_LOGE(TAG, "flight computer append error: %s", append_res.error()->what());
            //}

            memset(buffer, 'X', sizeof(buffer));
            idx = 0;
        }

    }
    // fixme: with finite loops we could lose data at the end
    // this will happen with infinite loops too, but there its kind of unavoidable

    fclose(data_file);
}

}