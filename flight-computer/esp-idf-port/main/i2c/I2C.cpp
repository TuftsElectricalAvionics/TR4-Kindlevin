#include "I2C.h"

#include <memory>
#include <utility>
#include <expected>

#include "driver/i2c_master.h"

namespace seds {
    constexpr uint32_t i2c_baudrate = 100'000;

    I2C::I2C(Private) {
        // These values are mostly pulled from the ESP IDF example for I2C.
        // The main difference is which GPIO ports are used.
        constexpr auto bus_config = i2c_master_bus_config_t {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = GPIO_NUM_33,
            .scl_io_num = GPIO_NUM_25,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags = {
                .enable_internal_pullup = true,
            },
        };

        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &this->bus_handle));
    }

    I2C::~I2C() {
        ESP_ERROR_CHECK(i2c_del_master_bus(this->handle()));
    }

    Result<I2CDevice> I2C::get_device(uint16_t address) {
        auto [_, did_insert] = this->used_addresses.insert(address);

        // Don't allow duplicate devices to prevent two subsystems writing to the same place
        // and causing interference.
        if (!did_insert) {
            return std::unexpected(
                std::make_unique<std::runtime_error>("Address in use")
            );
        }

        ESP_LOGI("i2c", "Making I2CDevice");

        return I2CDevice(this->shared_from_this(), address);
    }

    I2CDevice::I2CDevice(std::shared_ptr<I2C> bus, uint16_t address)
        : bus(std::move(bus)),
          address(address) {
        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = i2c_baudrate,
        };

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(this->bus->handle(), &dev_config, &this->dev_handle)
        );
    }

    I2CDevice::~I2CDevice() {
        // If this was moved, `bus` will be null.
        // (In that case, there's no need to free anything.)
        if (this->bus) {
            ESP_ERROR_CHECK(i2c_master_bus_rm_device(this->handle()));
            this->bus->used_addresses.erase(this->address);
        }
    }
}
