#include "sd.h"
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_vfs_fat_sdmmc_mount_config_t mount_cfg;
sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;
sdmmc_host_t host;
spi_bus_config_t bus_cfg;
sdspi_device_config_t slot_config;

namespace seds {

Expected<SDCard> SDCard::create() {
    mount_cfg = {
        .format_if_mount_failed = false,//true,
        .max_files = 5,//MAX_FILES,
        .allocation_unit_size = 16 * 1024, // this needs to match whatever we format it with
        .disk_status_check_enable = false,
        .use_one_fat = false,
    };
    
    host = SDSPI_HOST_DEFAULT();

    bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    ESP_TRY(spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA));
    ESP_LOGI("SD", "spi bus initialization successful");
    slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_TRY(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_cfg, &card));
    ESP_LOGI("SD", "fs mount successful");
    sdmmc_card_print_info(stdout, card);

    return SDCard();
}

struct log_args {
    FILE* file;
    uint8_t* buffer;
    size_t buf_len;
    std::atomic<size_t> *insert_idx;
    size_t min_size;
};

// pulled out to function to make code clearer
// shouldn't be slow since it's one line and we tell c++ to inline
// maybe optimize this to avoid branches? probably doesn't matter though
inline size_t wrap_around_diff(int32_t x, int32_t y, int32_t modulus) {
    if (x > y) {
        return (size_t)(x - y);
    }
    return (size_t)(x + modulus - y); 
}

void logging_task(void *arg_ptr) {
    struct log_args args = *(struct log_args *)arg_ptr;
    size_t remove_idx = 0;
    while (true) {
        size_t insert_idx = args.insert_idx->load(std::memory_order_relaxed); // prevent changing out from under us
        if (wrap_around_diff((int32_t)insert_idx, (int32_t)remove_idx, (int32_t)args.buf_len) < args.min_size) { continue; }
        // todo - have specific name for tag?
        if (insert_idx > remove_idx) {
            ESP_LOGI("sd_log_task", "%.*s", insert_idx - remove_idx, &args.buffer[remove_idx]);
        } else {
            // log in two parts
            ESP_LOGI("sd_log_task", "%.*s", args.buf_len - remove_idx, &args.buffer[remove_idx]);
            ESP_LOGI("sd_log_task", "%.*s", insert_idx, &args.buffer[0]);
        }
        remove_idx = insert_idx;
    }
}

Expected<std::monostate> SDCard::create_log_task(const char* path, uint8_t* buffer, size_t buf_len, std::atomic<size_t> *insert_idx, size_t min_size) {
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

    // must be created on heap
    // could be statically allocated somewhere, but I don't think that's necessary
    struct log_args *args = (struct log_args*)malloc(sizeof(log_args));

    args->file = f;
    args->buffer = buffer;
    args->buf_len = buf_len;
    args->insert_idx = insert_idx;
    args->min_size = min_size;

    // Force this task on the other core
    // TODO: give option to specify core?
    auto t_core = 1;
    if (CONFIG_ESP_MAIN_TASK_AFFINITY == t_core) {
        t_core = 0;
    }

    int priority = 1; // same than main task

    xTaskCreatePinnedToCore(logging_task, "logging_task", 4096, (void*)args, priority, NULL, t_core);

    return {};
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
