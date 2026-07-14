#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

constexpr const char* DEST = "0x5e9ee1089755c3435139848e47e6635505d5a13a";
constexpr const char* TOKEN = "USDC:0x8f254b963e8468305d409b33aa137c67";

// Pin the signature to one produced by the Python SDK with the same key and
// action, so a typo in the eight sign-type entries or the primary type string
// (invisible to shape-only tests) fails loudly:
//   sign_send_asset_action(wallet,
//       {"type": "sendAsset", "destination": DEST, "sourceDex": "",
//        "destinationDex": "spot", "token": TOKEN, "amount": "12.5",
//        "fromSubAccount": "", "nonce": 1583838}, is_mainnet=True)
void sendAssetSignatureMatchesPythonSdk() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::json action = {
        {"type", "sendAsset"},
        {"destination", DEST},
        {"sourceDex", ""},
        {"destinationDex", "spot"},
        {"token", TOKEN},
        {"amount", "12.5"},
        {"fromSubAccount", ""},
        {"nonce", 1583838}
    };

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"sourceDex", "string"},
        {"destinationDex", "string"},
        {"token", "string"},
        {"amount", "string"},
        {"fromSubAccount", "string"},
        {"nonce", "uint64"}
    };

    auto sig = hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:SendAsset", /*is_mainnet=*/true);

    const auto json = sig.toJson();
    assert(json.at("r") == "0x864cc853a704cd0371b318b00a6de9f03a40c997223381ae5482bcc3ae04d37e");
    assert(json.at("s") == "0x172c368a5ee32941b3a0fc84c8607a60e37866ce80f49ee17f58eb420949a9b8");
    assert(json.at("v") == 27);
}

void sendAssetPostsExpectedAction() {
    TestExchange exchange;

    exchange.sendAsset(DEST, "", "spot", TOKEN, 12.5);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "sendAsset");
    assert(action.at("destination") == DEST);
    assert(action.at("sourceDex") == "");
    assert(action.at("destinationDex") == "spot");
    assert(action.at("token") == TOKEN);
    assert(action.at("amount") == "12.5");  // floatToWire string, not a number
    assert(action.at("fromSubAccount") == "");
    assert(action.at("nonce").is_number_integer());

    // User-signed action: action nonce must match the envelope nonce.
    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));

    // postAction omits vaultAddress for sendAsset.
    assert(!payload.contains("vaultAddress"));
}

void sendAssetSendsVaultAsFromSubAccount() {
    const std::string vault = "0x1719884eb866cb12b2287399b15f7db5e7d775ea";
    TestExchange exchange(vault);

    exchange.sendAsset(DEST, "spot", "", TOKEN, 1.0);

    const auto& action = exchange.lastAction();
    assert(action.at("fromSubAccount") == vault);
    assert(action.at("amount") == "1");  // trailing zeros stripped
    assert(!exchange.lastPayload().contains("vaultAddress"));
}

} // namespace

int main() {
    sendAssetSignatureMatchesPythonSdk();
    sendAssetPostsExpectedAction();
    sendAssetSendsVaultAsFromSubAccount();

    return 0;
}
