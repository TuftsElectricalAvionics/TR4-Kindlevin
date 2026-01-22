#include "uart.h"

namespace seds {
    UART::UART(Private) {
        used_ports = std::set<uart_port_t>();
    }

    UART::~UART() {
        ESP_ERROR_CHECK(i2c_del_master_bus(this->handle()));
    }

    Expected<UARTDevice> UART::get_device(uart_port_t port, uint32_t baudrate, int tx, int rx, int rts, int cts, TickType_t delay, uint32_t buf_size) {
        auto [_, did_insert] = this->used_ports.insert(port);

        // Don't allow duplicate devices to prevent two subsystems writing to the same place
        // and causing interference.
        if (!did_insert) {
            return std::unexpected(
                std::make_unique<std::runtime_error>("Port in use")
            );
        }

        uart_config_t uart_config  = {
            .baud_rate = baudrate,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT,
        };

        ESP_LOGI("uart", "Making UARTDevice");

        ESP_TRY(uart_driver_install(port, buf_size * 2, 0, 0, NULL, 0));
        ESP_TRY(uart_param_config(port, &uart_config));
        ESP_TRY(uart_set_pin(port, tx, rx, rts, cts));

        return UARTDevice(this->shared_from_this(), port, delay);
    }

    UARTDevice::UARTDevice(std::shared_ptr<UART> bus, uart_port_t port, TickType_t t_delay) {
        this->bus = bus;
        this->port = port;
        this->delay = t_delay;
    }

    UARTDevice::~UARTDevice() {
        if (this->bus) {
            ESP_ERROR_CHECK(uart_driver_delete(this->port));
            this->bus->used_ports.erase(this->port);
        }
    }

    Expected<std::monostate> UARTDevice::write(const void* data, size_t length) {
        ESP_TRY(uart_write_bytes(this->port, data, length));
        return std::monostate {};
    }


    Expected<size_t> UARTDevice::read(uint8_t* buffer, size_t length, TickType_t ticks_to_wait, bool fail_on_underread) {
        int len = uart_read_bytes(this->port, buffer, length, ticks_to_wait / portTICK_PERIOD_MS);
        if (len == -1) {
            return std::unexpected(
                std::make_unique<std::runtime_error>("UART device read failed")
            );
        }
        if (fail_on_underread && len < length) {
            return std::unexpected(
                std::make_unique<std::runtime_error>("UART device read fewer bytes than expected")
            );
        }

        return (size_t)len;
    }


}