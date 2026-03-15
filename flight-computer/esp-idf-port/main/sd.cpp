#include "sd.h"
#include <string.h>

#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SD";

esp_vfs_fat_sdmmc_mount_config_t mount_cfg;
sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;
sdmmc_host_t host;
spi_bus_config_t bus_cfg;
sdspi_device_config_t slot_config;
spi_host_device_t host_slot;

namespace seds {

// making changes based on 
// https://github.com/mdarveau/SD_BENCHMARK_V2/blob/main/src/main.c

Expected<void> setup_spi_and_mount() {
    host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 26000;
    host_slot = (spi_host_device_t)host.slot;

    bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 128 * 1024,
    };

    // Ensure CS idles high prior to bus init
    gpio_config_t cs_cfg = {
        .pin_bit_mask = 1ULL << PIN_NUM_CS,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_cfg);
    gpio_set_level(PIN_NUM_CS, 1);

    ESP_TRY(spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "spi bus initialization successful");
    // Allow shifters/power to settle before card init
    vTaskDelay(pdMS_TO_TICKS(20));

    mount_cfg = {
        .format_if_mount_failed = false,//true,
        .max_files = 5, //MAX_FILES,
        .allocation_unit_size = 0,
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    
    
    slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_TRY(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_cfg, &card));
    ESP_LOGI(TAG, "fs mount successful");
    sdmmc_card_print_info(stdout, card);

    return {};
}

Expected<SDCard> SDCard::create() {
    TRY(setup_spi_and_mount());

    // from source, doing it twice makes things faster?
    esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
    card = NULL;
    spi_bus_free(host_slot);

    ESP_LOGE(TAG, "unmounted, now remounting");
    vTaskDelay(pdMS_TO_TICKS(20));

    TRY(setup_spi_and_mount());

    return SDCard();
}

struct log_args_inner {
    FILE* file;
    struct log_args args;
};

void logging_task(void *arg_ptr) {
    struct log_args_inner args_in = *(struct log_args_inner *)arg_ptr;
    FILE* file = args_in.file;

    uint8_t *stdio_buf = NULL;
    size_t bytes = VFS_STDIO_BUF_PREFERRED_KB * 1024;
    stdio_buf = (uint8_t *)heap_caps_aligned_alloc(32, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (stdio_buf) {
        setvbuf(file, (char *)stdio_buf, _IOFBF, bytes);
    } else {
        ESP_LOGE(TAG, "NO INTERNAL STDIO BUFFER");
        fclose(file);
        return;
    }

    struct log_args args = args_in.args;

    size_t remove_idxs[2] = {0, 0};
    struct timeval tv_now;
    size_t prev_which = 1;
    while (true) {
        if (!args.sd_sem->load(std::memory_order_seq_cst)) {
            args.write_sem->store(true, std::memory_order_seq_cst);
            // write to other buffer
            size_t which = (args.which_buffer->load(std::memory_order_relaxed) + 1) % 2;

            gettimeofday(&tv_now, NULL);
            int64_t time_ms = (int64_t)tv_now.tv_sec * 1000L + (int64_t)tv_now.tv_usec / 1000L;

            size_t insert_idx = args.insert_idxs[which].load(std::memory_order_relaxed); // prevent changing out from under us
            if (prev_which != which) {
                remove_idxs[which] = 0;
                prev_which = which;
            }
            if (insert_idx > remove_idxs[which]) { 
                size_t write_size = std::min(insert_idx - remove_idxs[which], args.write_size);
                ESP_LOGI("sd_log_task", "%lld: which: %zu, idx: %zu", time_ms, which, remove_idxs[which]);
                ESP_LOGI("sd_log_task", "%.*s", write_size, &args.buffers[which][remove_idxs[which]]);

                fwrite(&args.buffers[which][remove_idxs[which]], 1, write_size, file);

                fflush(file);
                fsync(fileno(file));
                remove_idxs[which] += write_size;
            }
            args.write_sem->store(false, std::memory_order_seq_cst);
       } else {
            args.write_sem->store(false, std::memory_order_seq_cst);
       }
    }
}

Expected<TaskHandle_t> SDCard::create_log_task(struct log_args call_args) {
    errno = 0;
    FILE *f = fopen(call_args.path, "a");
    if (f == NULL) {
        // save error
        auto error = errno;
        if (error == ENOMEM) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("failed to allocate memory for file")));
        }
        if (error == EDQUOT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EINVAL) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file basename was invalid")));
        }
        if (error == EISDIR) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file points to directory")));
        }
        if (error == ENAMETOOLONG) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("path was too long")));
        }
        if (error == ENOENT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("a directory in the path did not exist")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    // must be created on heap
    // could be statically allocated somewhere, but I don't think that's necessary
    struct log_args_inner *args = (struct log_args_inner*)malloc(sizeof(log_args_inner));

    args->file = f;
    args->args = call_args;

    // Force this task on the other core
    // TODO: give option to specify core?
    auto t_core = 1;
    if (CONFIG_ESP_MAIN_TASK_AFFINITY == t_core) {
        t_core = 0;
    }

    int priority = 1; // same than main task
    TaskHandle_t handle;
    xTaskCreatePinnedToCore(logging_task, "logging_task", 4096, (void*)args, priority, &handle, t_core);

    return handle;
}


Expected<std::monostate> SDCard::create_file(const char* path, const uint8_t* data, size_t length) {
    errno = 0;
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        // save error
        auto error = errno;
        if (error == ENOMEM) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("failed to allocate memory for file")));
        }
        if (error == EDQUOT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EINVAL) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file basename was invalid")));
        }
        if (error == EISDIR) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file points to directory")));
        }
        if (error == ENAMETOOLONG) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("path was too long")));
        }
        if (error == ENOENT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("a directory in the path did not exist")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    ESP_LOGI("SD", "created file");

    ssize_t written = fwrite(data, 1, length, f);
    if (written == -1) {
        // save error
        auto error = errno;
        clearerr(f);
        // attempt to close file
        fclose(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }
    if (written < length) {
        // TODO: Make these error enums so we can handle this case differently
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("wrote fewer bytes than expected")));
    }

    if (fclose(f) != 0) {
        // save error
        auto error = errno;
        clearerr(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    return {};
}

/*
Expected<std::monostate> SDCard::append_file(const char *path, const uint8_t *data, size_t length) {
    errno = 0;
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        // save error
        auto error = errno;
        if (error == ENOMEM) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("failed to allocate memory for file")));
        }
        if (error == EDQUOT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EINVAL) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file basename was invalid")));
        }
        if (error == EISDIR) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file points to directory")));
        }
        if (error == ENAMETOOLONG) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("path was too long")));
        }
        if (error == ENOENT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("a directory in the path did not exist")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    ssize_t written = fwrite(data, 1, length, f);
    if (written == -1) {
        // save error
        auto error = errno;
        clearerr(f);
        // attempt to close file
        fclose(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }
    if (written < length) {
        // TODO: Make these error enums so we can handle this case differently
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("wrote fewer bytes than expected")));
    }

    if (fclose(f) != 0) {
        // save error
        auto error = errno;
        clearerr(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    return {};
}

Expected<std::monostate> SDCard::read_file(const char *path, uint8_t* buffer, size_t length) {
    errno = 0;
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        // save error
        auto error = errno;
        if (error == ENOMEM) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("failed to allocate memory for file")));
        }
        if (error == EINVAL) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file basename was invalid")));
        }
        if (error == ENAMETOOLONG) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("path was too long")));
        }
        if (error == ENOENT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file does not exist")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    size_t read = fread(buffer, 1, length, f);
    if (ferror(f) != 0) {
        // save error
        auto error = errno;
        clearerr(f);
        // attempt to close file
        fclose(f);
        if (error == EISDIR) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file points to directory")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }
    if (read < length) {
        // TODO: Make these error enums so we can handle this case differently
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("read fewer bytes than expected")));
    }
    if (fclose(f) == EOF) {
        // save error
        auto error = errno;
        clearerr(f);
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            //return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(errno)));
        }
    };

    return {};
}

Expected<std::monostate> SDCard::flush_file(const char *path) {
    errno = 0;
    FILE *f = fopen(path, "a");
    if (f == NULL) {
        // save error
        auto error = errno;
        if (error == ENOMEM) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("failed to allocate memory for file")));
        }
        if (error == EDQUOT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EINVAL) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file basename was invalid")));
        }
        if (error == EISDIR) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file points to directory")));
        }
        if (error == ENAMETOOLONG) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("path was too long")));
        }
        if (error == ENOENT) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("a directory in the path did not exist")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    fflush(f);
    if (errno != 0) {
        // save error
        auto error = errno;
        clearerr(f);
        // attempt to close file
        fclose(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    if (fclose(f) != 0) {
        // save error
        auto error = errno;
        clearerr(f);
        if (error == EDQUOT || error == ENOSPC) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("no space on card for file")));
        }
        if (error == EIO) {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("an io error occured")));
        }
        else {
            return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error(strerror(error))));
        }
    }

    return {};
}

Expected<std::monostate> SDCard::rename_file(const char* old_name, const char* new_name) {
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(old_name, &st) != 0) {
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("old file does not exist")));
    }

    if (stat(new_name, &st) == 0) {
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("new file already exists")));
    }

    if (rename(old_name, new_name) != 0) {
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("Renaming file failed")));
    }

    return {};
}

Expected<std::monostate> SDCard::format_fatfs() {
    ESP_TRY(esp_vfs_fat_sdcard_format(MOUNT_POINT, card));
    return {};
}

Expected<struct stat> SDCard::stat_file(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return std::unexpected(std::make_unique<std::runtime_error>(std::runtime_error("file does not exist")));
    }

    return st;
}
*/

}
