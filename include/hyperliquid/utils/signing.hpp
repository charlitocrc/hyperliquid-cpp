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
 * Note: Uses ordered_json to preserve key insertion order for msgpack serialization
 */
Signature signL1Action(const Wallet& wallet,
                      const nlohmann::ordered_json& action,
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
 * Enrich a user-signed action's EIP-712 types with the two multi-sig fields.
 *
 * "payloadMultiSigUser" and "outerSigner" are inserted directly after
 * "hyperliquidChain", which is where the exchange expects them.
 *
 * Throws if the types carry no "hyperliquidChain" entry. The Python SDK only
 * prints a warning and returns the types unchanged, which produces a
 * well-formed signature that no one can verify -- a failure that only shows up
 * as a rejected action much later.
 */
std::vector<EIP712Type> addMultiSigTypes(const std::vector<EIP712Type>& sign_types);

/**
 * Add the two multi-sig fields to a user-signed action, returning a copy.
 * Both addresses are lowercased, as the exchange signs over the lowercase form.
 */
nlohmann::json addMultiSigFields(nlohmann::json action,
                                 const std::string& payload_multi_sig_user,
                                 const std::string& outer_signer);

/**
 * Sign a user-signed inner action (usdSend, sendAsset, convertToMultiSigUser,
 * ...) as one authorized user of a multi-sig account.
 *
 * Each authorized user runs this over the same action, nonce and outer signer;
 * the collected signatures are then passed to Exchange::multiSig().
 *
 * The action is not modified: unlike a plain user-signed action, the caller
 * must put "signatureChainId" ("0x66eee") and "hyperliquidChain"
 * ("Mainnet"/"Testnet") into the action they later hand to multiSig(), since
 * the exchange verifies the inner signature against the action as sent.
 *
 * @param action Inner action, including signatureChainId and hyperliquidChain
 * @param sign_types The action's normal EIP-712 types; the multi-sig fields
 *                   are added here, so pass the same list a single-signer call
 *                   would use
 * @param primary_type e.g. "HyperliquidTransaction:SendAsset"
 * @param payload_multi_sig_user The multi-sig account the action acts on
 * @param outer_signer The authorized user (or its agent) that will send the
 *                     final multiSig action -- the leader
 */
Signature signMultiSigUserSignedActionPayload(const Wallet& wallet,
                                              const nlohmann::json& action,
                                              const std::vector<EIP712Type>& sign_types,
                                              const std::string& primary_type,
                                              const std::string& payload_multi_sig_user,
                                              const std::string& outer_signer,
                                              bool is_mainnet);

/**
 * Sign an L1 inner action (order, cancel, updateLeverage, ...) as one
 * authorized user of a multi-sig account.
 *
 * The signed payload is the array [payloadMultiSigUser, outerSigner, action],
 * not the action alone, so a signature collected here cannot be replayed as an
 * ordinary single-signer action.
 *
 * Key order in `action` is preserved into the msgpack hash, so build it the way
 * the exchange orders it -- orderWiresToOrderAction() and the Exchange methods
 * already do.
 *
 * The nonce, vault address and expires_after must be identical across all
 * signers and must match what Exchange::multiSig() later sends.
 */
Signature signMultiSigL1ActionPayload(const Wallet& wallet,
                                      const nlohmann::ordered_json& action,
                                      const std::optional<std::string>& vault_address,
                                      int64_t nonce,
                                      std::optional<int64_t> expires_after,
                                      const std::string& payload_multi_sig_user,
                                      const std::string& outer_signer,
                                      bool is_mainnet);

/**
 * Sign the outer multiSig action, which wraps the collected signatures.
 * Signed by the leader only; Exchange::multiSig() does this for you.
 *
 * The hash covers the action with its "type" tag removed, and is then signed as
 * a user-signed HyperliquidTransaction:SendMultiSig payload.
 */
Signature signMultiSigAction(const Wallet& wallet,
                             const nlohmann::ordered_json& multi_sig_action,
                             const std::optional<std::string>& vault_address,
                             int64_t nonce,
                             std::optional<int64_t> expires_after,
                             bool is_mainnet);

/**
 * Build the "signers" field of a convertToMultiSigUser action: a JSON *string*
 * holding the sorted authorized users and the signature threshold.
 *
 * Exchange::convertToMultiSigUser() calls this. Call it directly only when
 * changing an existing multi-sig account's signer set, which has to travel
 * through multiSig() as an inner action.
 *
 * Addresses are validated and lowercased, then sorted. Throws on an empty list,
 * duplicates, more than 10 users, or a threshold outside [1, users].
 */
std::string convertToMultiSigUserSigners(const std::vector<std::string>& authorized_users,
                                         int threshold);

/**
 * Compute action hash: keccak256(msgpack(action) + nonce + vault + expires)
 * Note: Uses ordered_json to preserve key insertion order for msgpack serialization
 */
std::vector<uint8_t> actionHash(const nlohmann::ordered_json& action,
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
 * Returns ordered_json to preserve key insertion order for L1 action signing
 */
nlohmann::ordered_json orderWiresToOrderAction(const std::vector<OrderWire>& order_wires,
                                              const std::optional<BuilderInfo>& builder,
                                              const std::string& grouping);

} // namespace hyperliquid
