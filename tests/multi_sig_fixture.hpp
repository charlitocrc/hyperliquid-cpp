#pragma once

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

} // namespace
