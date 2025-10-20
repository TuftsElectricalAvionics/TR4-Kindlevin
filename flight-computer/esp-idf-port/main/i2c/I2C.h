#pragma once
#include <expected>
#include <memory>
#include <set>
#include <chrono>
#include <algorithm>
#include <string>
#include <bit>
#include <type_traits>

#include "driver/i2c_types.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "errors.h"
#include "esp_log.h"
#include "utils.h"

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
        /// Returns a shared I2C bus object.
        ///
        /// Attempting to create multiple I2C buses will abort the program.
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
        Expected<I2CDevice> get_device(uint16_t address);

        /// Returns a handle to an I2C device using its default address.
        template<typename Device>
        Expected<Device> get_device() {
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

        /// Perform a write-read transaction on the I2C bus. The given byte buffer is written
        /// to the device and then `ReadN` bytes are read back.
        template<size_t ReadN, size_t WriteN>
        Expected<std::array<uint8_t, ReadN>> write_read(
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

        /// Write a byte buffer to the I2C bus. No bytes are read back.
        template<size_t WriteN>
        [[nodiscard]]
        Expected<std::monostate> write(
            std::array<uint8_t, WriteN> const& write_buf
        ) {
            ESP_TRY(
                i2c_master_transmit(
                    this->handle(),
                    write_buf.data(),
                    write_buf.size(),
                    // I'm not totally sure why we need to divide by the tick period here, but it
                    // was in the I2C example.
                    timeout.count() / portTICK_PERIOD_MS
                )
            );

            return std::monostate {};
        }

        /// Read a big-endian number from the given register of this I2C device.
        ///
        /// To make it easier to keep track of register constants, you can pass in a custom register
        /// enum variant as long as it can be statically cast to a `uint8_t`.
        template<typename ReadT, typename RegisterT>
        [[nodiscard]]
        Expected<ReadT> read_be_register(RegisterT const reg) {
            static_assert(std::is_arithmetic_v<ReadT>, "ReadT must be a number");

            auto write_buf = std::array { static_cast<uint8_t>(reg) };
            auto read_buf = TRY(this->write_read<sizeof(ReadT)>(write_buf));

            return num::from_be_bytes<ReadT>(read_buf);
        }

        /// Write a big-endian number to the given register of this I2C device.
        ///
        /// To make it easier to keep track of register constants, you can pass in a custom register
        /// enum variant as long as it can be statically cast to a `uint8_t`.
        template<typename WriteT, typename RegisterT>
        [[nodiscard]]
        Expected<std::monostate> write_be_register(RegisterT const reg, WriteT const new_value) {
            static_assert(std::is_arithmetic_v<WriteT>, "WriteT must be a number");

            std::array<uint8_t, 1 + sizeof(WriteT)> write_buf = {
                static_cast<uint8_t>(reg),
                // ...temporarily unfilled
            };

            // Write new value's bytes into the write buffer.
            std::ranges::copy(num::to_be_bytes(new_value), &write_buf[1]);

            return this->write(write_buf);
        }

        /// Read a little-endian number from the given register of this I2C device.
        ///
        /// To make it easier to keep track of register constants, you can pass in a custom register
        /// enum variant as long as it can be statically cast to a `uint8_t`.
        template<typename ReadT, typename RegisterT>
        [[nodiscard]]
        Expected<ReadT> read_le_register(RegisterT const reg) {
            static_assert(std::is_arithmetic_v<ReadT>, "ReadT must be a number");

            auto write_buf = std::array { static_cast<uint8_t>(reg) };
            auto read_buf = TRY(this->write_read<sizeof(ReadT)>(write_buf));

            return num::from_le_bytes<ReadT>(read_buf);
        }

        /// Write a little-endian number to the given register of this I2C device.
        ///
        /// To make it easier to keep track of register constants, you can pass in a custom register
        /// enum variant as long as it can be statically cast to a `uint8_t`.
        template<typename WriteT, typename RegisterT>
        [[nodiscard]]
        Expected<std::monostate> write_le_register(RegisterT const reg, WriteT const new_value) {
            static_assert(std::is_arithmetic_v<WriteT>, "WriteT must be a number");

            std::array<uint8_t, 1 + sizeof(WriteT)> write_buf = {
                static_cast<uint8_t>(reg),
                // ...temporarily unfilled
            };

            // Write new value's bytes into the write buffer.
            std::ranges::copy(num::to_le_bytes(new_value), &write_buf[1]);

            return this->write(write_buf);
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
