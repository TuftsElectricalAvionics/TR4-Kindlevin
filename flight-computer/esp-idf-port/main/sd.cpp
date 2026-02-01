#include "sd.h"

namespace seds {

Expected<SDCard> SDCard::create() {
    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = MAX_FILES,
        .allocation_unit_size = 16 * 1024 // taken from example
    };

    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // TODO: Do we need to do this? 
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    // TODO: Are we using an on-board power supply, or is it coming from the SD card module?
    // If the latter, we need to set that port

    // TODO: What are quadwp/hd settings?
    // What is are max transfer sz?
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    ESP_TRY(spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA));

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    // TODO: Check
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    ESP_TRY(esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_cfg, &card));

    return SDCard(card, host);
}

Expected<std::monostate> SDCard::create_file(const char* path, const uint8_t* data, size_t length) {
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("failed to open file for writing")));
    }

    fwrite(data, 1, length, f);
    fclose(f);

    // TODO CHECK ERRORS!

    return {};
}

Expected<std::monostate> read_file(const char *path, uint8_t* buffer, size_t length) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("failed to open file for writing")));
    }

    fread(buffer, 1, length, f);
    fclose(f);
    // TODO CHECK ERROR

    return {};
}

Expected<std::monostate> SDCard::rename_file(const char* old_name, const char* new_name) {
    // Check if destination file exists before renaming
    struct stat st;
    if (stat(old_name, &st) != 0) {
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("old file does not exist")));
    }

    if (stat(new_name, &st) == 0) {
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("new file already exists")));
    }

    if (rename(old_name, new_name) != 0) {
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("Renaming file failed")));
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
        return std::unexpected(std::make_unique<std::exception>(std::runtime_error("file does not exist")));
    }

    return st;
}

}
