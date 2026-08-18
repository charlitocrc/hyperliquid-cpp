// Cover for the multi-sig signing path: the inner payloads each authorized
// user signs, the outer envelope the leader signs, and the two Exchange
// actions that produce them.
//
// Every signature below is pinned to one the Python SDK produced from the same
// key, action and nonce. That matters more here than elsewhere: the multi-sig
// payloads are assembled from pieces (an enriched type list, an array envelope,
// a hash-of-a-hash) that a shape-only test would happily accept while the
// exchange rejects them.

#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using hyperliquid::EIP712Type;
using hyperliquid::Signature;
using hyperliquid::Wallet;
using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

// The leader: TEST_PRIVATE_KEY's own address.
constexpr const char* LEADER = "0x14791697260e4c9a71f18484c9f997b308e59325";

// A second authorized user, signing the inner payloads.
constexpr const char* SIGNER_KEY =
    "0x1123456789012345678901234567890123456789012345678901234567890123";

constexpr const char* MULTI_SIG_USER = "0x0000000000000000000000000000000000000005";

constexpr const char* VAULT = "0x1234567890abcdef1234567890abcdef12345678";

constexpr int64_t NONCE = 1583838;

// Ordered exactly as the exchange orders an order action; key order feeds the
// msgpack hash.
nlohmann::ordered_json innerOrderAction() {
    nlohmann::ordered_json order;
    order["a"] = 4;
    order["b"] = true;
    order["p"] = "1100";
    order["s"] = "0.2";
    order["r"] = false;
    order["t"] = nlohmann::ordered_json{{"limit", nlohmann::ordered_json{{"tif", "Gtc"}}}};

    nlohmann::ordered_json action;
    action["type"] = "order";
    action["orders"] = nlohmann::ordered_json::array({order});
    action["grouping"] = "na";
    return action;
}

std::vector<EIP712Type> sendAssetSignTypes() {
    return {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"sourceDex", "string"},
        {"destinationDex", "string"},
        {"token", "string"},
        {"amount", "string"},
        {"fromSubAccount", "string"},
        {"nonce", "uint64"}
    };
}

// The two multi-sig fields land right after hyperliquidChain, and nowhere else.
void addMultiSigTypesInsertsAfterHyperliquidChain() {
    auto enriched = hyperliquid::addMultiSigTypes(sendAssetSignTypes());

    assert(enriched.size() == sendAssetSignTypes().size() + 2);
    assert(enriched[0].name == "hyperliquidChain");
    assert(enriched[1].name == "payloadMultiSigUser");
    assert(enriched[1].type == "address");
    assert(enriched[2].name == "outerSigner");
    assert(enriched[2].type == "address");
    assert(enriched[3].name == "destination");
    assert(enriched.back().name == "nonce");
}

// Python only warns here and returns the types unchanged, which yields a
// signature nothing can verify. We refuse to produce it.
void addMultiSigTypesThrowsWithoutHyperliquidChain() {
    bool threw = false;
    try {
        hyperliquid::addMultiSigTypes({{"amount", "string"}, {"nonce", "uint64"}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void addMultiSigFieldsLowercasesAndKeepsAction() {
    nlohmann::json action = {{"type", "sendAsset"}, {"amount", "12.5"}};

    auto enriched = hyperliquid::addMultiSigFields(
        action, MULTI_SIG_USER, "0x14791697260E4c9A71f18484C9f997B308e59325");

    assert(enriched.at("payloadMultiSigUser") == MULTI_SIG_USER);
    assert(enriched.at("outerSigner") == LEADER);
    assert(enriched.at("amount") == "12.5");
    // The caller's action is untouched, so it can still be sent as-is.
    assert(!action.contains("outerSigner"));
}

// Pinned to the Python SDK:
//   sign_multi_sig_l1_action_payload(wallet, inner_order, True, None, 1583838,
//       None, MULTI_SIG_USER, LEADER)
void multiSigL1PayloadSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(SIGNER_KEY);

    auto sig = hyperliquid::signMultiSigL1ActionPayload(
        *wallet, innerOrderAction(), std::nullopt, NONCE, std::nullopt,
        MULTI_SIG_USER, LEADER, /*is_mainnet=*/true).toJson();

    assert(sig.at("r") == "0x70c30c9dcb9ca1cc104997cba902eae08f6423771bfd6a15af800ac6786976eb");
    assert(sig.at("s") == "0x4b9f4c4c961873cb98a5cc4d49662a93e5619e01693958e07e48789667d49c4b");
    assert(sig.at("v") == 28);
}

// Pinned to the Python SDK:
//   sign_multi_sig_user_signed_action_payload(wallet, inner_send_asset, True,
//       SEND_ASSET_SIGN_TYPES, "HyperliquidTransaction:SendAsset",
//       MULTI_SIG_USER, LEADER)
void multiSigUserSignedPayloadSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(SIGNER_KEY);

    nlohmann::json inner = {
        {"type", "sendAsset"},
        {"signatureChainId", "0x66eee"},
        {"hyperliquidChain", "Mainnet"},
        {"destination", "0x5e9ee1089755c3435139848e47e6635505d5a13a"},
        {"sourceDex", ""},
        {"destinationDex", "spot"},
        {"token", "USDC:0x8f254b963e8468305d409b33aa137c67"},
        {"amount", "12.5"},
        {"fromSubAccount", ""},
        {"nonce", NONCE}
    };

    auto sig = hyperliquid::signMultiSigUserSignedActionPayload(
        *wallet, inner, sendAssetSignTypes(), "HyperliquidTransaction:SendAsset",
        MULTI_SIG_USER, LEADER, /*is_mainnet=*/true).toJson();

    assert(sig.at("r") == "0xe7837b7c8680e93a91fcdb6d1c17011014bcd547752d3baee828f709c4e2b54d");
    assert(sig.at("s") == "0x1d346b635a4390257bb273c422acf0548125cd09c48997f6ec32a64b1d2a9489");
    assert(sig.at("v") == 27);

    // Signing must not mutate the caller's action -- multiSig() sends the same
    // object, and an extra field there would change the hash the leader signs.
    assert(!inner.contains("payloadMultiSigUser"));
    assert(!inner.contains("outerSigner"));
}

// The outer envelope. Pinned to the Python SDK:
//   sign_multi_sig_action(wallet, multi_sig_action, True, <vault>, 1583838, None)
void multiSigActionSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::ordered_json inner_signature;
    inner_signature["r"] = "0x70c30c9dcb9ca1cc104997cba902eae08f6423771bfd6a15af800ac6786976eb";
    inner_signature["s"] = "0x4b9f4c4c961873cb98a5cc4d49662a93e5619e01693958e07e48789667d49c4b";
    inner_signature["v"] = 28;

    nlohmann::ordered_json payload;
    payload["multiSigUser"] = MULTI_SIG_USER;
    payload["outerSigner"] = LEADER;
    payload["action"] = innerOrderAction();

    nlohmann::ordered_json action;
    action["type"] = "multiSig";
    action["signatureChainId"] = "0x66eee";
    action["signatures"] = nlohmann::ordered_json::array({inner_signature});
    action["payload"] = payload;

    auto no_vault = hyperliquid::signMultiSigAction(
        *wallet, action, std::nullopt, NONCE, std::nullopt, /*is_mainnet=*/true).toJson();
    assert(no_vault.at("r") == "0xc219372eb83ebc8bb00d21d27958e10accb0829980d363eccf7bcf9fe547fa42");
    assert(no_vault.at("s") == "0x486eb6269a97ed05c1897a0c4b9bc5c046aa7aba3ce7b7aedcd498bef794b364");
    assert(no_vault.at("v") == 27);

    // The vault is part of the hashed preimage, not just the envelope.
    auto with_vault = hyperliquid::signMultiSigAction(
        *wallet, action, std::string(VAULT), NONCE, std::nullopt, /*is_mainnet=*/true).toJson();
    assert(with_vault.at("r") == "0xd3947040a2798bef0b2e6ad7d9b7dc43b1eeaa0da5ec79caf89c37c987f137e8");
    assert(with_vault.at("s") == "0x78b80886c2a139b002d2ae7a59274b3b04cdac3251154b861e63ca00165d6910");
    assert(with_vault.at("v") == 28);
}

// Byte-for-byte what Python's json.dumps produces, spacing included: the
// exchange signs this as an opaque string, so the bytes are the contract.
void convertToMultiSigUserSignersMatchesPythonSdk() {
    auto signers = hyperliquid::convertToMultiSigUserSigners(
        {"0x0000000000000000000000000000000000000001",
         "0x0000000000000000000000000000000000000000"},
        2);

    assert(signers ==
           "{\"authorizedUsers\": [\"0x0000000000000000000000000000000000000000\", "
           "\"0x0000000000000000000000000000000000000001\"], \"threshold\": 2}");
}

void convertToMultiSigUserSignersRejectsUnusableSets() {
    auto throws = [](const std::vector<std::string>& users, int threshold) {
        try {
            hyperliquid::convertToMultiSigUserSigners(users, threshold);
        } catch (const std::invalid_argument&) {
            return true;
        }
        return false;
    };

    const std::string user = "0x0000000000000000000000000000000000000001";
    const std::string other = "0x0000000000000000000000000000000000000002";

    assert(throws({}, 1));                    // no signers
    assert(throws({user}, 0));                // threshold below 1
    assert(throws({user, other}, 3));         // unreachable threshold: bricked account
    assert(throws({user, user}, 2));          // duplicate cannot sign twice
    assert(throws({"0xnothex"}, 1));          // malformed address
    assert(throws(std::vector<std::string>(11, user), 1));  // over the 10-user cap
}

void convertToMultiSigUserPostsSortedSigners() {
    TestExchange exchange;

    exchange.convertToMultiSigUser({"0x0000000000000000000000000000000000000001",
                                    "0x0000000000000000000000000000000000000000"},
                                   2);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "convertToMultiSigUser");
    assert(action.at("signers") ==
           "{\"authorizedUsers\": [\"0x0000000000000000000000000000000000000000\", "
           "\"0x0000000000000000000000000000000000000001\"], \"threshold\": 2}");
    assert(action.at("nonce").is_number_integer());

    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));
}

// Recompute the envelope signature independently and compare, which pins every
// input multiSig() feeds the signer: action body, vault, nonce, expiry, chain.
void multiSigPostsExpectedActionAndSignature() {
    TestExchange exchange;

    Signature inner_signature{
        "0x70c30c9dcb9ca1cc104997cba902eae08f6423771bfd6a15af800ac6786976eb",
        "0x4b9f4c4c961873cb98a5cc4d49662a93e5619e01693958e07e48789667d49c4b",
        28};

    exchange.multiSig(MULTI_SIG_USER, innerOrderAction(), {inner_signature}, NONCE);

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "multiSig");
    assert(action.at("signatureChainId") == "0x66eee");
    assert(action.at("signatures").size() == 1);
    assert(action.at("signatures")[0].at("r") == inner_signature.r);

    const auto& payload = action.at("payload");
    assert(payload.at("multiSigUser") == MULTI_SIG_USER);
    assert(payload.at("outerSigner") == LEADER);
    assert(payload.at("action").at("type") == "order");
    assert(payload.at("action").at("orders")[0].at("p") == "1100");

    const auto& envelope = exchange.lastPayload();
    // The caller's nonce, not a fresh one: every signer signed over it.
    assert(envelope.at("nonce") == NONCE);
    assert(envelope.at("vaultAddress").is_null());

    nlohmann::ordered_json expected_action;
    expected_action["type"] = "multiSig";
    expected_action["signatureChainId"] = "0x66eee";
    expected_action["signatures"] =
        nlohmann::ordered_json::array({nlohmann::ordered_json(inner_signature.toJson())});
    nlohmann::ordered_json expected_payload;
    expected_payload["multiSigUser"] = MULTI_SIG_USER;
    expected_payload["outerSigner"] = LEADER;
    expected_payload["action"] = innerOrderAction();
    expected_action["payload"] = expected_payload;

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    // TestExchange points at localhost, so is_mainnet is false here.
    auto expected = hyperliquid::signMultiSigAction(
        *wallet, expected_action, std::nullopt, NONCE, std::nullopt,
        /*is_mainnet=*/false).toJson();

    assert(envelope.at("signature") == expected);
}

// A configured vault must be signed over and sent, or the exchange verifies a
// signature against a preimage the request does not describe.
void multiSigSignsAndSendsConfiguredVault() {
    TestExchange exchange(VAULT);

    Signature inner_signature{"0x01", "0x02", 27};
    exchange.multiSig(MULTI_SIG_USER, innerOrderAction(), {inner_signature}, NONCE);

    const auto& envelope = exchange.lastPayload();
    assert(envelope.at("vaultAddress") == VAULT);

    nlohmann::ordered_json expected_action;
    expected_action["type"] = "multiSig";
    expected_action["signatureChainId"] = "0x66eee";
    expected_action["signatures"] =
        nlohmann::ordered_json::array({nlohmann::ordered_json(inner_signature.toJson())});
    nlohmann::ordered_json expected_payload;
    expected_payload["multiSigUser"] = MULTI_SIG_USER;
    expected_payload["outerSigner"] = LEADER;
    expected_payload["action"] = innerOrderAction();
    expected_action["payload"] = expected_payload;

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    auto expected = hyperliquid::signMultiSigAction(
        *wallet, expected_action, std::string(VAULT), NONCE, std::nullopt,
        /*is_mainnet=*/false).toJson();

    assert(envelope.at("signature") == expected);
}

void multiSigRejectsUnusableInput() {
    TestExchange exchange;

    bool threw = false;
    try {
        exchange.multiSig(MULTI_SIG_USER, innerOrderAction(), {}, NONCE);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        exchange.multiSig(MULTI_SIG_USER, nlohmann::ordered_json{{"orders", nlohmann::json::array()}},
                          {Signature{"0x01", "0x02", 27}}, NONCE);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        exchange.multiSig("0xshort", innerOrderAction(), {Signature{"0x01", "0x02", 27}}, NONCE);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    assert(exchange.requests().empty());
}

} // namespace

int main() {
    addMultiSigTypesInsertsAfterHyperliquidChain();
    addMultiSigTypesThrowsWithoutHyperliquidChain();
    addMultiSigFieldsLowercasesAndKeepsAction();
    multiSigL1PayloadSignatureMatchesPythonSdk();
    multiSigUserSignedPayloadSignatureMatchesPythonSdk();
    multiSigActionSignatureMatchesPythonSdk();
    convertToMultiSigUserSignersMatchesPythonSdk();
    convertToMultiSigUserSignersRejectsUnusableSets();
    convertToMultiSigUserPostsSortedSigners();
    multiSigPostsExpectedActionAndSignature();
    multiSigSignsAndSendsConfiguredVault();
    multiSigRejectsUnusableInput();

    return 0;
}
