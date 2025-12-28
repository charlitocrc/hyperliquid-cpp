#pragma once

#include <stdexcept>
#include <string>

namespace hyperliquid {

/**
 * Base exception class for Hyperliquid SDK errors
 */
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
};

/**
 * Client error (4xx HTTP status codes)
 */
class ClientError : public Error {
public:
    ClientError(int status_code,
               const std::string& error_code,
               const std::string& error_message,
               const std::string& error_data = "")
        : Error(error_message),
          status_code_(status_code),
          error_code_(error_code),
          error_data_(error_data) {}

    int statusCode() const { return status_code_; }
    const std::string& errorCode() const { return error_code_; }
    const std::string& errorData() const { return error_data_; }

private:
    int status_code_;
    std::string error_code_;
    std::string error_data_;
};

/**
 * Server error (5xx HTTP status codes)
 */
class ServerError : public Error {
public:
    ServerError(int status_code, const std::string& message)
        : Error(message), status_code_(status_code) {}

    int statusCode() const { return status_code_; }

private:
    int status_code_;
};

} // namespace hyperliquid
