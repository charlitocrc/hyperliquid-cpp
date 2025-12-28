#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace hyperliquid {

// Cloid implementation

Cloid::Cloid(const std::string& raw) : raw_cloid_(raw) {
    validate();
}

Cloid Cloid::fromInt(uint64_t value) {
    std::ostringstream oss;
    oss << "0x" << std::setfill('0') << std::setw(32) << std::hex << value;
    return Cloid(oss.str());
}

Cloid Cloid::fromStr(const std::string& hex) {
    std::string normalized = hex;
    if (hex.substr(0, 2) != "0x") {
        normalized = "0x" + hex;
    }
    return Cloid(normalized);
}

void Cloid::validate() {
    if (raw_cloid_.length() != 34) {
        throw std::invalid_argument("Cloid must be 34 characters (0x + 32 hex chars)");
    }
    if (raw_cloid_.substr(0, 2) != "0x") {
        throw std::invalid_argument("Cloid must start with 0x");
    }
    // Validate hex characters
    for (size_t i = 2; i < raw_cloid_.length(); ++i) {
        char c = raw_cloid_[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            throw std::invalid_argument("Cloid contains invalid hex characters");
        }
    }
}

// OrderType implementation

nlohmann::json OrderType::toJson() const {
    nlohmann::json result;
    if (limit.has_value()) {
        result["limit"] = limit->toJson();
    }
    if (trigger.has_value()) {
        result["trigger"] = trigger->toJson();
    }
    return result;
}

// OrderWire implementation

nlohmann::json OrderWire::toJson() const {
    nlohmann::json result = {
        {"a", asset},
        {"b", is_buy},
        {"p", price},
        {"s", size},
        {"r", reduce_only},
        {"t", order_type}
    };
    if (cloid.has_value()) {
        result["c"] = cloid.value();
    }
    return result;
}

} // namespace hyperliquid
