#pragma once

#include <expected>
#include <print>
#include <iostream>
#include "esp_log.h"

// This file defines helpers for exceptionless error handling.
// (Exceptions are disabled in ESP-IDF.)
// Functions can explicitly mark themselves as fallible by returning a Result. Consumers
// of these functions can either handle the possible error with the std::expected API or propagate
// it with the TRY macro or crash with a helpful message usage the unwrap function.

/// Returns std::unexpected if the given ESP-IDF call failed.
#define ESP_TRY(x) ({                                                                               \
    esp_err_t err_rc_ = (x);                                                                        \
    if (err_rc_ != ESP_OK) {                                                                        \
        std::unique_ptr<std::exception> err = std::make_unique<seds::errors::EspError>(err_rc_);    \
        return std::unexpected(std::move(err));                                                     \
    }                                                                                               \
})

/// Returns std::unexpected if the given std::expected value is an error.
#define TRY(x) ({                                                                                   \
    auto result = (x);                                                                              \
    if (!result.has_value()) {                                                                      \
        return std::unexpected(std::move(result).error());                                          \
    }                                                                                               \
    std::move(result.value());                                                                      \
})

namespace seds::errors {
    /// Contains either a value or an error. An alias for std::expected with a dynamically-typed
    /// error.
    template<typename T>
    using Result = std::expected<T, std::unique_ptr<std::exception>>;

    /// An error sourced from ESP-IDF.
    class EspError final : public std::runtime_error {
    public:
        explicit EspError(esp_err_t const error) : runtime_error(esp_err_to_name(error)) {}
    };

    /// Checks if the given value is an error, and if it is, terminates.
    ///
    /// This function should be used to prevent further execution in the event of an
    /// unrecoverable error.
    template<typename T>
    T unwrap(Result<T>&& expected) {
        if (expected.has_value()) {
            return std::move(*expected);
        }

        ESP_LOGE("unwrap", "Fatal Error: %s", expected.error()->what());
        std::terminate();
    }
}
