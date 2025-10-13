#pragma once
#include <expected>
#include <memory>
#include <set>
#include <chrono>
#include <algorithm>
#include <string>

#include "driver/i2c_types.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "errors.h"
#include "esp_log.h"

namespace seds {
    using namespace std::chrono_literals;
    using namespace seds::errors;

    class I2CDevice;

    /// An I2C bus.
    class I2C : public std::enable_shared_from_this<I2C> {
        // Used to prevent construction except by create().
        // I2C bus objects need to be managed by a shared ptr so that they can give out strong
        // references to new devices.
        struct Private {
            explicit Private() = default;
        };

    public:
        static std::shared_ptr<I2C> create() {
            return std::make_shared<I2C>(Private());
        }

        explicit I2C(Private);
        ~I2C();


        /// Returns a handle to an I2C device.
        ///
        /// Each address can only be used once with this method, so there's a guarantee that
        /// each device produced by this is unique and you can send data without interference.
        ///
        /// Returns an error if the caller tried to create two devices with the same address.
        [[nodiscard]]
        Result<I2CDevice> get_device(uint16_t address);

        /// Returns a handle to an I2C device using its default address.
        template<typename Device>
        Result<Device> get_device() {
            ESP_LOGI("i2c", "Getting device with address %x", Device::default_address);
            auto handle = TRY(this->get_device(Device::default_address));
            ESP_LOGI("i2c", "Got device");

            return Device(std::move(handle));
        }

        /// Returns the I2C handle so that peripherals can access the bus.
        [[nodiscard]]
        i2c_master_bus_handle_t handle() const {
            return this->bus_handle;
        }

    private:
        friend class I2CDevice;

        i2c_master_bus_handle_t bus_handle { nullptr };
        std::set<uint16_t> used_addresses;
    };

    /// A unique I2C device. It is a type invariant that there is no other I2C device
    /// with this address on the same bus.
    class I2CDevice {
    public:
        static constexpr auto timeout = 1000ms;

        // Disallow accidentally making duplicates
        I2CDevice(I2CDevice const&) = delete;
        I2CDevice& operator=(I2CDevice const&) = delete;
        I2CDevice(I2CDevice&&) = default;
        I2CDevice& operator=(I2CDevice&&) = default;

        ~I2CDevice();

        /// Performs a write-read transaction on the I2C bus. The given byte buffer is written
        /// to the device and then `ReadN` bytes are read back.
        template<size_t ReadN, size_t WriteN>
        Result<std::array<uint8_t, ReadN>> write_read(
            std::array<uint8_t, WriteN> const& write_buf
        ) {
            std::array<uint8_t, ReadN> read_buf;

            ESP_TRY(
                i2c_master_transmit_receive(
                    this->handle(),
                    write_buf.data(),
                    write_buf.size(),
                    read_buf.data(),
                    read_buf.size(),
                    // I'm not totally sure why we need to divide by the tick period here, but it
                    // was in the I2C example.
                    timeout.count() / portTICK_PERIOD_MS
                )
            );

            return read_buf;
        }

        /// Reads the data at the given register from the I2C device.
        template<typename ReadT, typename RegisterT>
        [[nodiscard]]
        Result<ReadT> read_register(RegisterT reg) {
            auto write_buf = std::array { static_cast<uint8_t>(reg) };
            auto read_buf = TRY(this->write_read<sizeof(ReadT)>(write_buf));

            // We reverse the order of the bytes because I2C uses big-endian, but we want
            // little-endian as that's what our CPU uses.
            std::ranges::reverse(read_buf);

            // Cast the byte buffer to the actual integer type now.
            return std::bit_cast<ReadT>(read_buf);
        }

        [[nodiscard]]
        std::shared_ptr<I2C> get_bus() const {
            return this->bus;
        }

        [[nodiscard]]
        i2c_master_dev_handle_t handle() const {
            return this->dev_handle;
        }

    private:
        // This constructor is called from I2C::get_device, which has some extra checks.
        friend class I2C;
        I2CDevice(std::shared_ptr<I2C> bus, uint16_t address);

        std::shared_ptr<I2C> bus;
        uint16_t address;
        i2c_master_dev_handle_t dev_handle { nullptr };
    };
}
