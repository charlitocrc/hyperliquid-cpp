#pragma once

#include "hyperliquid/types.hpp"
#include <memory>
#include <vector>
#include <nlohmann/json.hpp>

namespace hyperliquid {

/**
 * Wallet class for managing private keys and signing
 */
class Wallet {
public:
    /**
     * Create wallet from hex private key (with or without "0x" prefix)
     */
    static std::shared_ptr<Wallet> fromPrivateKey(const std::string& private_key_hex);

    /**
     * Get the Ethereum address derived from this wallet's public key
     */
    std::string address() const;

    /**
     * Sign a message hash with ECDSA
     */
    Signature signMessage(const std::vector<uint8_t>& message_hash) const;

    ~Wallet();

private:
    explicit Wallet(void* ec_key);  // EC_KEY* hidden from header

    void* ec_key_;  // OpenSSL EC_KEY*
    std::string address_;
};

/**
 * Sign an L1 action (orders, cancels, etc.) using EIP-712
 */
Signature signL1Action(const Wallet& wallet,
                      const nlohmann::json& action,
                      const std::optional<std::string>& vault_address,
                      int64_t nonce,
                      std::optional<int64_t> expires_after,
                      bool is_mainnet);

/**
 * Sign a user-signed action (transfers, etc.) using EIP-712
 */
Signature signUserSignedAction(const Wallet& wallet,
                               nlohmann::json action,
                               const std::vector<EIP712Type>& payload_types,
                               const std::string& primary_type,
                               bool is_mainnet);

/**
 * Compute action hash: keccak256(msgpack(action) + nonce + vault + expires)
 */
std::vector<uint8_t> actionHash(const nlohmann::json& action,
                                const std::optional<std::string>& vault_address,
                                int64_t nonce,
                                std::optional<int64_t> expires_after);

/**
 * Construct phantom agent for L1 action signing
 */
nlohmann::json constructPhantomAgent(const std::vector<uint8_t>& hash, bool is_mainnet);

/**
 * Create EIP-712 payload for L1 actions
 */
nlohmann::json l1Payload(const nlohmann::json& phantom_agent);

/**
 * Create EIP-712 payload for user-signed actions
 */
nlohmann::json userSignedPayload(const std::string& primary_type,
                                const std::vector<EIP712Type>& payload_types,
                                const nlohmann::json& action);

/**
 * Convert OrderRequest to OrderWire format
 */
OrderWire orderRequestToOrderWire(const OrderRequest& order, int asset);

/**
 * Create order action from order wires
 */
nlohmann::json orderWiresToOrderAction(const std::vector<OrderWire>& order_wires,
                                      const std::optional<BuilderInfo>& builder,
                                      const std::string& grouping);

} // namespace hyperliquid
