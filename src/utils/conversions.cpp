#include "hyperliquid/utils/conversions.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace hyperliquid {

std::string floatToWire(double value) {
    // Format to 8 decimal places
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << value;
    std::string rounded = oss.str();

    // Verify no significant rounding occurred
    double back = std::stod(rounded);
    if (std::abs(back - value) >= 1e-12) {
        throw std::runtime_error("floatToWire causes rounding");
    }

    // Handle -0 case
    if (rounded == "-0.00000000") {
        rounded = "0.00000000";
    }

    // Normalize: remove trailing zeros
    size_t decimal_pos = rounded.find('.');
    if (decimal_pos != std::string::npos) {
        // Remove trailing zeros
        size_t last_nonzero = rounded.find_last_not_of('0');
        if (last_nonzero >= decimal_pos) {
            rounded = rounded.substr(0, last_nonzero + 1);
        }
        // Remove decimal point if no fractional part
        if (rounded.back() == '.') {
            rounded.pop_back();
        }
    }

    return rounded;
}

int64_t floatToUsdInt(double value) {
    return floatToInt(value, 6);
}

int64_t floatToInt(double value, int decimals) {
    double multiplier = std::pow(10.0, decimals);
    return static_cast<int64_t>(std::round(value * multiplier));
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::string hex_str = hex;

    // Remove "0x" prefix if present
    if (hex_str.length() >= 2 && hex_str.substr(0, 2) == "0x") {
        hex_str = hex_str.substr(2);
    }

    // Hex string must have even length
    if (hex_str.length() % 2 != 0) {
        throw std::invalid_argument("Hex string must have even length");
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(hex_str.length() / 2);

    for (size_t i = 0; i < hex_str.length(); i += 2) {
        std::string byte_str = hex_str.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

std::string bytesToHex(const std::vector<uint8_t>& bytes, bool with_prefix) {
    return bytesToHex(bytes.data(), bytes.size(), with_prefix);
}

std::string bytesToHex(const uint8_t* data, size_t len, bool with_prefix) {
    std::ostringstream oss;
    if (with_prefix) {
        oss << "0x";
    }
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string normalizeAddress(const std::string& address) {
    std::string normalized = address;

    // Convert to lowercase
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    // Validate format
    if (normalized.length() != 42 || normalized.substr(0, 2) != "0x") {
        throw std::invalid_argument("Invalid Ethereum address format");
    }

    return normalized;
}

int64_t getTimestampMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace hyperliquid
