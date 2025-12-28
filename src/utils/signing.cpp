#include "hyperliquid/utils/signing.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <msgpack.hpp>
#include <sstream>
#include <stdexcept>

namespace hyperliquid {

// Forward declarations from crypto namespace
namespace crypto {
    std::vector<uint8_t> keccak256(const std::vector<uint8_t>& data);
    void* createKeyFromPrivate(const std::string& private_key_hex);
    std::string deriveAddress(const void* ec_key);
    Signature signHash(const void* ec_key, const std::vector<uint8_t>& hash);
    void freeKey(void* ec_key);
    std::vector<uint8_t> encodeTypedData(const nlohmann::json& typed_data);
}

// Helper function to pack JSON to msgpack
static void packJson(msgpack::packer<std::stringstream>& packer, const nlohmann::json& j) {
    if (j.is_null()) {
        packer.pack_nil();
    } else if (j.is_boolean()) {
        packer.pack(j.get<bool>());
    } else if (j.is_number_integer()) {
        packer.pack(j.get<int64_t>());
    } else if (j.is_number_unsigned()) {
        packer.pack(j.get<uint64_t>());
    } else if (j.is_number_float()) {
        packer.pack(j.get<double>());
    } else if (j.is_string()) {
        packer.pack(j.get<std::string>());
    } else if (j.is_array()) {
        packer.pack_array(j.size());
        for (const auto& item : j) {
            packJson(packer, item);
        }
    } else if (j.is_object()) {
        packer.pack_map(j.size());
        for (auto it = j.begin(); it != j.end(); ++it) {
            packer.pack(it.key());
            packJson(packer, it.value());
        }
    }
}

// Wallet implementation

Wallet::Wallet(void* ec_key) : ec_key_(ec_key) {
    address_ = crypto::deriveAddress(ec_key_);
}

Wallet::~Wallet() {
    crypto::freeKey(ec_key_);
}

std::shared_ptr<Wallet> Wallet::fromPrivateKey(const std::string& private_key_hex) {
    void* ec_key = crypto::createKeyFromPrivate(private_key_hex);
    return std::shared_ptr<Wallet>(new Wallet(ec_key));
}

std::string Wallet::address() const {
    return address_;
}

Signature Wallet::signMessage(const std::vector<uint8_t>& message_hash) const {
    return crypto::signHash(ec_key_, message_hash);
}

// Action hash computation

std::vector<uint8_t> actionHash(const nlohmann::json& action,
                                const std::optional<std::string>& vault_address,
                                int64_t nonce,
                                std::optional<int64_t> expires_after) {
    std::vector<uint8_t> data;

    // 1. Msgpack serialize the action
    std::stringstream ss;
    msgpack::packer<std::stringstream> packer(ss);
    packJson(packer, action);
    std::string msgpack_str = ss.str();
    data.insert(data.end(), msgpack_str.begin(), msgpack_str.end());

    // 2. Append nonce (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }

    // 3. Append vault address if present
    if (!vault_address.has_value()) {
        data.push_back(0x00);
    } else {
        data.push_back(0x01);
        std::vector<uint8_t> addr_bytes = hexToBytes(vault_address.value());
        if (addr_bytes.size() != 20) {
            throw std::runtime_error("Invalid vault address length");
        }
        data.insert(data.end(), addr_bytes.begin(), addr_bytes.end());
    }

    // 4. Append expires_after if present
    if (expires_after.has_value()) {
        data.push_back(0x00);
        int64_t expires = expires_after.value();
        for (int i = 7; i >= 0; --i) {
            data.push_back(static_cast<uint8_t>((expires >> (i * 8)) & 0xFF));
        }
    }

    // 5. Hash with Keccak-256
    return crypto::keccak256(data);
}

// Phantom agent construction

nlohmann::json constructPhantomAgent(const std::vector<uint8_t>& hash, bool is_mainnet) {
    std::string source = is_mainnet ? "a" : "b";
    std::string connection_id = bytesToHex(hash, true);

    return {
        {"source", source},
        {"connectionId", connection_id}
    };
}

// L1 payload for EIP-712

nlohmann::json l1Payload(const nlohmann::json& phantom_agent) {
    nlohmann::json payload = {
        {"domain", {
            {"name", "Exchange"},
            {"version", "1"},
            {"chainId", 1337},
            {"verifyingContract", "0x0000000000000000000000000000000000000000"}
        }},
        {"primaryType", "Agent"},
        {"types", {
            {"EIP712Domain", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}},
                {{"name", "version"}, {"type", "string"}},
                {{"name", "chainId"}, {"type", "uint256"}},
                {{"name", "verifyingContract"}, {"type", "address"}}
            })},
            {"Agent", nlohmann::json::array({
                {{"name", "source"}, {"type", "string"}},
                {{"name", "connectionId"}, {"type", "bytes32"}}
            })}
        }},
        {"message", phantom_agent}
    };

    return payload;
}

// User-signed payload for EIP-712

nlohmann::json userSignedPayload(const std::string& primary_type,
                                const std::vector<EIP712Type>& payload_types,
                                const nlohmann::json& action) {
    nlohmann::json types_array = nlohmann::json::array();
    for (const auto& type : payload_types) {
        types_array.push_back(type.toJson());
    }

    nlohmann::json payload = {
        {"domain", {
            {"name", "HyperliquidSignTransaction"},
            {"version", "1"},
            {"chainId", 0x66eee},  // Fixed chain ID for user-signed actions
            {"verifyingContract", "0x0000000000000000000000000000000000000000"}
        }},
        {"primaryType", primary_type},
        {"types", {
            {"EIP712Domain", nlohmann::json::array({
                {{"name", "name"}, {"type", "string"}},
                {{"name", "version"}, {"type", "string"}},
                {{"name", "chainId"}, {"type", "uint256"}},
                {{"name", "verifyingContract"}, {"type", "address"}}
            })},
            {primary_type, types_array}
        }},
        {"message", action}
    };

    return payload;
}

// Sign L1 action

Signature signL1Action(const Wallet& wallet,
                      const nlohmann::json& action,
                      const std::optional<std::string>& vault_address,
                      int64_t nonce,
                      std::optional<int64_t> expires_after,
                      bool is_mainnet) {
    // Compute action hash
    auto hash = actionHash(action, vault_address, nonce, expires_after);

    // Construct phantom agent
    auto phantom_agent = constructPhantomAgent(hash, is_mainnet);

    // Create EIP-712 payload
    auto payload = l1Payload(phantom_agent);

    // Encode typed data
    auto message_hash = crypto::encodeTypedData(payload);

    // Sign the hash
    return wallet.signMessage(message_hash);
}

// Sign user-signed action

Signature signUserSignedAction(const Wallet& wallet,
                               nlohmann::json action,
                               const std::vector<EIP712Type>& payload_types,
                               const std::string& primary_type,
                               bool is_mainnet) {
    // Add hyperliquidChain to action
    action["hyperliquidChain"] = is_mainnet ? "Mainnet" : "Testnet";

    // Create EIP-712 payload
    auto payload = userSignedPayload(primary_type, payload_types, action);

    // Encode typed data
    auto message_hash = crypto::encodeTypedData(payload);

    // Sign the hash
    return wallet.signMessage(message_hash);
}

// Order conversion

OrderWire orderRequestToOrderWire(const OrderRequest& order, int asset) {
    OrderWire wire;
    wire.asset = asset;
    wire.is_buy = order.is_buy;
    wire.price = floatToWire(order.limit_px);
    wire.size = floatToWire(order.sz);
    wire.reduce_only = order.reduce_only;

    // Convert order type
    if (order.order_type.limit.has_value()) {
        wire.order_type = {
            {"limit", {{"tif", order.order_type.limit->tif}}}
        };
    } else if (order.order_type.trigger.has_value()) {
        wire.order_type = {
            {"trigger", {
                {"triggerPx", floatToWire(order.order_type.trigger->trigger_px)},
                {"isMarket", order.order_type.trigger->is_market},
                {"tpsl", order.order_type.trigger->tpsl}
            }}
        };
    }

    // Add cloid if present
    if (order.cloid.has_value()) {
        wire.cloid = order.cloid->toRaw();
    }

    return wire;
}

// Create order action

nlohmann::json orderWiresToOrderAction(const std::vector<OrderWire>& order_wires,
                                      const std::optional<BuilderInfo>& builder,
                                      const std::string& grouping) {
    nlohmann::json orders_array = nlohmann::json::array();
    for (const auto& wire : order_wires) {
        orders_array.push_back(wire.toJson());
    }

    nlohmann::json action = {
        {"type", "order"},
        {"orders", orders_array},
        {"grouping", grouping}
    };

    if (builder.has_value()) {
        action["builder"] = {
            {"b", builder->b},
            {"f", builder->f}
        };
    }

    return action;
}

} // namespace hyperliquid
