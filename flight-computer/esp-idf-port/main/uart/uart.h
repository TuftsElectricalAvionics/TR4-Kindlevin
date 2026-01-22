#pragma once

#include <expected>
#include <set>

#include "driver/uart.h"
#include "driver/gpio.h"
#include "errors.h"

namespace seds {
    using namespace seds::errors;

    class UARTDevice;

    /// A UART interface.
    class UART : public std::enable_shared_from_this<UART> {
    
        // This uses the same structure as I2C
        struct Private {
            explicit Private() = default;
        };

    public:

        static std::shared_ptr<UART> create() {
            return std::make_shared<UART>(Private());
        }

        explicit UART(Private);

        ~UART();

        /// Returns a handle to a UART device
        ///
        /// Each address can only be used once with this method, so there's a guarantee that
        /// each device produced by this is unique and you can send data without interference.
        ///
        /// Returns an error if the caller tried to create two devices with the same address.
        /// Reading from the UARTDevice already accounts for dividing by portTICK_PERIOD_MS, 
        /// so don't include that in the calculation of `delay`
        [[nodiscard]]
        Expected<UARTDevice> get_device(uart_port_t port, uint32_t baudrate, int tx, int rx, int rts, int cts, TickType_t delay, uint32_t buf_size);

        /// Returns a handle to an UART device using its default address.
        template<typename Device>
        Expected<Device> get_device() {
            ESP_LOGI("uart", "Getting device with address %x", Device::default_address);
            auto handle = TRY(this->get_device(Device::default_address));
            ESP_LOGI("uart", "Got device");

            return Device(std::move(handle));
        }
    
        private:
            friend class UARTDevice;

            std::set<uart_port_t> used_ports;
    };

    /// A unique UART device. It is a type invariant that there is no other UART device
    /// with this address on the same bus.

    class UARTDevice {
    public:
        // Disallow accidentally making duplicates
        UARTDevice(UARTDevice const&) = delete;
        UARTDevice& operator=(UARTDevice const&) = delete;
        UARTDevice(UARTDevice&&) = default;
        UARTDevice& operator=(UARTDevice&&) = default;

        ~UARTDevice();

        /// Writes data to the UART.
        ///
        /// Returns an error if the write fails.
        Expected<std::monostate> write(const void* data, size_t length);

        

        /// Reads data from the UART.
        ///
        /// Returns the number of bytes read or an error if the read fails.
        /// `fail_on_underread` indicates that the function should error if fewer than `length` bytes are read
        // Should we check if the requested buffer is larger than the allocated buffer?
        Expected<size_t> read(uint8_t* buffer, size_t length, TickType_t ticks_to_wait = delay, bool fail_on_underread = false);

    private:
        friend class UART;
        UARTDevice(std::shared_ptr<UART> bus, uart_port_t port, TickType_t t_delay);

        std::shared_ptr<UART> bus;
        TickType_t delay;
        uart_port_t port;
    }
}