#include "computer.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/time.h>

static const char *TAG = "computer";

namespace seds {

Expected<std::monostate> FlightComputer::init() {
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

constexpr size_t LOOPS_BEFORE_FLUSH = 25;
const size_t MAX_BLOCK_SIZE = (40 + 14 * 20 + 13 + 1 + 1);
constexpr size_t BUF_LEN = LOOPS_BEFORE_FLUSH * MAX_BLOCK_SIZE;
char buffers[2][BUF_LEN];

void FlightComputer::process(uint32_t times, bool endless) {
    std::atomic<bool> sd_req = {false};
    std::atomic<bool> writing = {false};

    struct timeval tv_now;
    memset(buffers[0], 'X', sizeof(buffers[0]));
    memset(buffers[1], 'X', sizeof(buffers[1]));
    std::atomic<size_t> idxs[2] = {{0}, {0}};
    std::atomic<size_t> which_buf = {0};

    char data[] = "timestamp, accel x, accel y, accel z, degrees x, degrees y, degrees z, baro 1 temp, baro 1 pressure, baro 2 temp, baro 2 pressure, high g accel x, high g accel y, high g accel z, temp\n";
    auto err = this->sd.create_file(MOUNT_POINT"/test1.csv", (uint8_t *)data, sizeof(data)-1);

    if (!err.has_value()) {
        ESP_LOGE(TAG, "critical error when creating test file: %s", err.error()->what());
    }

    FILE* f = fopen(MOUNT_POINT"/test1.csv", "a");

    if (f == NULL) {
        ESP_LOGE(TAG, "file pointer was null!");
    }


    // spawn other task
    // change back to this->filename once we're done testing!
    struct log_args args = {
        .path = this->filename,
        .buffers = {(uint8_t*)buffers[0], (uint8_t*)buffers[1]},
        .insert_idxs = idxs,
        .which_buffer = &which_buf,
        .write_size = 1000,
        .sd_sem = &sd_req,
        .write_sem = &writing
    };
    TaskHandle_t handle = unwrap(this->sd.create_log_task(args));

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
        } else {
            ESP_LOGE(TAG, "baro 1 data read failed");
        }

        auto baro_2_data_try = this->baro2.read_data();
        if (baro_2_data_try.has_value()) {
            baro_2_data = baro_2_data_try.value();
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
        
        auto idx = &idxs[which_buf.load(std::memory_order_relaxed)];

        size_t inc = snprintf(&buffers[which_buf.load(std::memory_order_relaxed)][idx->load(std::memory_order_relaxed)], MAX_BLOCK_SIZE, "%lld,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g\n",
            time_ms,
            imu_data.ax, imu_data.ay, imu_data.az, imu_data.gx, imu_data.gy, imu_data.gz,
            baro_1_data.baro_temp, baro_1_data.pressure, baro_2_data.baro_temp, baro_2_data.pressure,
            high_g_data.h_ax, high_g_data.h_ay, high_g_data.h_az, tmp
        );
        ESP_LOGI(TAG, "%lld: which: %zu, idx: %zu,  %.*s", 
            time_ms, which_buf.load(std::memory_order_relaxed), idx->load(std::memory_order_relaxed),
            inc, &buffers[which_buf.load(std::memory_order_relaxed)][idx->load(std::memory_order_relaxed)] );
        
        idx->fetch_add(inc, std::memory_order_acq_rel);
        
        // no room left
        if (BUF_LEN - idx->load(std::memory_order_relaxed) <= MAX_BLOCK_SIZE) {
            ESP_LOGI(TAG, "FLUSHING");
            ESP_LOGI(TAG, "SUSPENDING OTHER TASK");

            // ensure the other task has done its sd stuff
            //while (xSemaphoreTake(sd_semaphore, pdMS_TO_TICKS(30)) != pdTRUE) {}

            sd_req.store(true, std::memory_order_seq_cst);
            while(writing.load(std::memory_order_seq_cst)) {
                // spin
                vTaskDelay(pdMS_TO_TICKS(10));
            };


            ESP_LOGI(TAG, "AQCUIRED SEMAPHORE!");
            // accurate compare threads by preventing other thread from catching up while we write
            vTaskSuspend(handle);
            ESP_LOGI(TAG, "SUSPENDED OTHER TASK");
            
            errno = 0;
            size_t write_idx = (size_t)idx->load(std::memory_order_seq_cst);
            ESP_LOGI(TAG, "Writing at idx %u", write_idx);
            fwrite((void *)buffers[which_buf.load(std::memory_order_relaxed)], 1, write_idx, f);
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }
            errno = 0;
            fflush(f);
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }
            errno = 0;
            fsync(fileno(f));
            if (errno != 0) {
                ESP_LOGE(TAG, "err: %s", strerror(errno));
            }

            ESP_LOGI(TAG, "sd write successful");

            size_t next_which = (which_buf.load(std::memory_order_relaxed) + 1) % 2;
            auto next_idx = &idxs[next_which];
            next_idx->store(0, std::memory_order_seq_cst);
            which_buf.store(next_which, std::memory_order_seq_cst);

            ESP_LOGI(TAG, "RESUMING OTHER TASK");
            sd_req.store(false, std::memory_order_seq_cst);
            vTaskResume(handle);
        }

        // allow printing task to overtake for now
        vTaskDelay(pdMS_TO_TICKS(150));
    }
}

}