#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

// Mixed-case on purpose: the EIP-712 "address" encoding (first used by this
// action) must hash the 20 raw bytes, so checksum casing cannot change the
// signature.
constexpr const char* BUILDER = "0x6B41600ce9d883eaE12e2Dd64CdCBeb6dc87c0fb";

// Pin the signature to one produced by the Python SDK with the same key and
// action:
//   sign_approve_builder_fee(wallet,
//       {"maxFeeRate": "0.001%", "builder": BUILDER, "nonce": 1583838,
//        "type": "approveBuilderFee"}, is_mainnet=True)
void approveBuilderFeeSignatureMatchesPythonSdk() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::json action = {
        {"type", "approveBuilderFee"},
        {"maxFeeRate", "0.001%"},
        {"builder", BUILDER},
        {"nonce", 1583838}
    };

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"maxFeeRate", "string"},
        {"builder", "address"},
        {"nonce", "uint64"}
    };

    auto sig = hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:ApproveBuilderFee", /*is_mainnet=*/true);

    const auto json = sig.toJson();
    assert(json.at("r") == "0xf96a0964e4f590e3a1eecc456594eb21ddeb3da57f16f6c524f0684e8c91a648");
    assert(json.at("s") == "0x68000193eb06d36e997385ad7afc0b0b58c688c8cc029cc7bdec32b756fd166f");
    assert(json.at("v") == 28);
}

void approveBuilderFeePostsExpectedAction() {
    TestExchange exchange;

    exchange.approveBuilderFee(BUILDER, "0.001%");

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "approveBuilderFee");
    assert(action.at("maxFeeRate") == "0.001%");
    assert(action.at("builder") == BUILDER);
    assert(action.at("nonce").is_number_integer());

    // User-signed action: action nonce must match the envelope nonce.
    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));
}

} // namespace

int main() {
    approveBuilderFeeSignatureMatchesPythonSdk();
    approveBuilderFeePostsExpectedAction();

    return 0;
}
