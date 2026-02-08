#pragma once

#include <expected>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <variant>

#include "errors.h"
#include "utils.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

const uint32_t MAX_FILES = 10; // ?
const gpio_num_t PIN_NUM_MOSI = (gpio_num_t)13;
const gpio_num_t PIN_NUM_MISO = (gpio_num_t)12; 
const gpio_num_t PIN_NUM_CLK = (gpio_num_t)14; 
const gpio_num_t PIN_NUM_CS = (gpio_num_t)5; 

// Normally, making a constant is preferred to using a definition
// Here, we want a definition for a specific purpose
// When a user wants to create a file, they must specify the path, including the mount path
// If we use a const, the normal path name must be concatenated with the actual path
// This requires either a fixed length buffer (bad for potential writing into uninitialized memory)
// Or malloc (bad for performance)
// Here, the user just writes MOUNT_POINT/path
#define MOUNT_POINT "/sdcard"

// Example reference:
// https://github.com/espressif/esp-idf/blob/v5.5.2/examples/storage/sd_card/sdspi/main/sd_card_example_main.c

namespace seds {
    using namespace seds::errors;

    class SDCard {
    public:
        ~SDCard() {
            esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
            spi_bus_free((spi_host_device_t)host.slot); // TODO CHANGE

            // Deinitialize the power control driver if it was used
            // TODO
        }

        static Expected<SDCard> create();

        Expected<std::monostate> create_file(const char* path, const uint8_t* data, size_t length);

        // Don't allocate for the user so that they have the option to use a predefined buffer
        /// Make sure to add a null-terminator if needed
        Expected<std::monostate> read_file(const char *path, uint8_t* buffer, size_t length);

        Expected<std::monostate> append_file(const char *path, const uint8_t* data, size_t length);

        /// If the old file doesn't exist, or the new file does, this function errors
        Expected<std::monostate> rename_file(const char* old_name, const char* new_name);

        Expected<std::monostate> format_fatfs();

        Expected<struct stat> stat_file(const char* path);
        
    private:
        SDCard(sdmmc_card_t *v_card, sdmmc_host_t v_host) :  card(v_card), host(v_host) {}
        sdmmc_card_t* card = NULL;
        sdmmc_host_t host;
    };
}