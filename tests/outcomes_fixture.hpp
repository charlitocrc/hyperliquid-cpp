#pragma once
// HIP-4 outcomes: coin/asset encoding, order rounding, and the userOutcome /
// outcome deployer actions.
//
// The Python SDK has no outcome support, so the wire shapes are pinned against
// the API docs. Each expected action below is the docs' JSON parsed into an
// ordered_json, and the request's signature is recomputed from it: L1 actions
// are signed over their msgpack bytes, so this fails if the SDK sends the right
// keys in a different order than the docs, not just if a field is wrong.

#include "test_exchange.hpp"

#include "hyperliquid/utils/conversions.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

template <typename F>
bool throws(F&& f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void assertSent(const TestExchange& exchange, const char* expected_json,
                bool with_vault = false, const std::string& vault = "") {
    const auto expected = nlohmann::ordered_json::parse(expected_json);
    const auto& payload = exchange.lastPayload();
    assert(payload.at("action") == nlohmann::json::parse(expected_json));

    const auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto signature = hyperliquid::signL1Action(
        *wallet, expected,
        with_vault ? std::optional<std::string>(vault) : std::nullopt,
        payload.at("nonce").get<int64_t>(), std::nullopt, /*is_mainnet=*/false);
    assert(payload.at("signature") == signature.toJson());
}

// Pins signL1Action() on `action_json` to the Python SDK's
//   sign_l1_action(wallet, action, None, 1583838, None, False)
// so assertSent() + this together tie the SDK's request to Python's.
// Python drops leading zeros from r/s, so compare as numbers.
void assertPythonSignature(const char* action_json, const char* r, const char* s, int v) {
    const auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto sig = hyperliquid::signL1Action(*wallet, nlohmann::ordered_json::parse(action_json),
                                               std::nullopt, 1583838, std::nullopt, false)
                         .toJson();
    const auto strip = [](std::string hex) {
        hex = hex.substr(2);
        return hex.substr(std::min(hex.find_first_not_of('0'), hex.size()));
    };
    assert(strip(sig.at("r")) == strip(r));
    assert(strip(sig.at("s")) == strip(s));
    assert(sig.at("v") == v);
}

// assertSent() checks the SDK against signL1Action(); this checks
// signL1Action() itself against the Python SDK on the outcome shapes (null
// amount, nested tuple arrays), with
//   sign_l1_action(wallet, action, None, 1583838, None, False)

} // namespace
