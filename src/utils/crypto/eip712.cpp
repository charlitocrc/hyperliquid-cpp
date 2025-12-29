#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace hyperliquid {
namespace crypto {

// Forward declare keccak256
std::vector<uint8_t> keccak256(const uint8_t* data, size_t len);
std::vector<uint8_t> keccak256(const std::vector<uint8_t>& data);

std::string encodeType(const std::string& primary_type,
                      const std::map<std::string, std::vector<EIP712Type>>& types) {
    auto it = types.find(primary_type);
    if (it == types.end()) {
        throw std::runtime_error("Primary type not found in types map");
    }

    std::string result = primary_type + "(";
    const auto& fields = it->second;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) result += ",";
        result += fields[i].type + " " + fields[i].name;
    }
    result += ")";
    return result;
}

std::vector<uint8_t> hashType(const std::string& primary_type,
                              const std::map<std::string, std::vector<EIP712Type>>& types) {
    std::string encoded = encodeType(primary_type, types);
    return keccak256(reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
}

std::vector<uint8_t> encodeField(const std::string& type, const nlohmann::json& value) {
    std::vector<uint8_t> encoded(32, 0);

    if (type == "string") {
        // Hash the string value
        std::string str = value.get<std::string>();
        auto hash = keccak256(reinterpret_cast<const uint8_t*>(str.data()), str.size());
        return hash;
    } else if (type == "bytes32") {
        // Convert hex string to bytes
        std::string hex_str = value.get<std::string>();
        auto bytes = hexToBytes(hex_str);
        if (bytes.size() != 32) {
            throw std::runtime_error("bytes32 field must be 32 bytes");
        }
        return bytes;
    } else if (type == "uint64") {
        // Encode as big-endian 32 bytes
        uint64_t num = value.get<uint64_t>();
        for (int i = 7; i >= 0; --i) {
            encoded[24 + (7 - i)] = (num >> (i * 8)) & 0xFF;
        }
        return encoded;
    } else if (type == "uint256") {
        // Encode as big-endian 32 bytes
        // Can handle both uint64 and larger numbers stored as uint64
        uint64_t num = value.get<uint64_t>();
        for (int i = 7; i >= 0; --i) {
            encoded[24 + (7 - i)] = (num >> (i * 8)) & 0xFF;
        }
        return encoded;
    } else if (type == "address") {
        // Address is 20 bytes, left-padded to 32 bytes
        std::string addr = value.get<std::string>();
        auto bytes = hexToBytes(addr);
        if (bytes.size() != 20) {
            throw std::runtime_error("Address must be 20 bytes");
        }
        std::copy(bytes.begin(), bytes.end(), encoded.begin() + 12);
        return encoded;
    } else {
        throw std::runtime_error("Unsupported EIP-712 type: " + type);
    }
}

std::vector<uint8_t> hashStruct(const std::string& struct_type,
                               const nlohmann::json& data,
                               const std::map<std::string, std::vector<EIP712Type>>& types) {
    // Start with type hash
    std::vector<uint8_t> encoded = hashType(struct_type, types);

    // Append encoded field values
    auto it = types.find(struct_type);
    if (it == types.end()) {
        throw std::runtime_error("Struct type not found in types map");
    }

    const auto& fields = it->second;
    for (const auto& field : fields) {
        if (!data.contains(field.name)) {
            throw std::runtime_error("Missing field in struct data: " + field.name);
        }
        std::vector<uint8_t> field_bytes = encodeField(field.type, data[field.name]);
        encoded.insert(encoded.end(), field_bytes.begin(), field_bytes.end());
    }

    return keccak256(encoded);
}

std::vector<uint8_t> encodeTypedData(const nlohmann::json& typed_data) {
    // EIP-712 prefix
    std::vector<uint8_t> result = {0x19, 0x01};

    // Extract components
    if (!typed_data.contains("types") || !typed_data.contains("domain") ||
        !typed_data.contains("primaryType") || !typed_data.contains("message")) {
        throw std::runtime_error("Invalid EIP-712 typed data structure");
    }

    // Build types map
    std::map<std::string, std::vector<EIP712Type>> types_map;
    for (auto& [type_name, fields] : typed_data["types"].items()) {
        std::vector<EIP712Type> field_list;
        for (auto& field : fields) {
            field_list.push_back({field["name"], field["type"]});
        }
        types_map[type_name] = field_list;
    }

    // Domain separator
    auto domain_hash = hashStruct("EIP712Domain", typed_data["domain"], types_map);
    result.insert(result.end(), domain_hash.begin(), domain_hash.end());

    // Message hash
    std::string primary_type = typed_data["primaryType"];
    auto message_hash = hashStruct(primary_type, typed_data["message"], types_map);
    result.insert(result.end(), message_hash.begin(), message_hash.end());

    return keccak256(result);
}

} // namespace crypto
} // namespace hyperliquid
