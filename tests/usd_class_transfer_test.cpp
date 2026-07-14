#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

// The EIP-712 bool encoding was added for this action's toPerp field, so pin
// the signature to one produced by the Python SDK with the same key and action:
//   sign_usd_class_transfer_action(wallet,
//       {"type": "usdClassTransfer", "amount": "12.5", "toPerp": True,
//        "nonce": 1583838}, is_mainnet=True)
void usdClassTransferSignatureMatchesPythonSdk() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::json action = {
        {"type", "usdClassTransfer"},
        {"amount", "12.5"},
        {"toPerp", true},
        {"nonce", 1583838}
    };

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"amount", "string"},
        {"toPerp", "bool"},
        {"nonce", "uint64"}
    };

    auto sig = hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:UsdClassTransfer", /*is_mainnet=*/true);

    const auto json = sig.toJson();
    assert(json.at("r") == "0x3a20bff08e666ab4b4ba366c811949a60c3b3e4ada6b78c0ae0af4ce9341d1f4");
    assert(json.at("s") == "0x1d194425ca4b9bd64db2319149ad62156bb3de8bf8a2ff944e9b06e5eff483ac");
    assert(json.at("v") == 28);
}

void usdClassTransferPostsExpectedAction() {
    TestExchange exchange;

    exchange.usdClassTransfer(12.5, /*to_perp=*/true);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "usdClassTransfer");
    assert(action.at("amount") == "12.5");  // floatToWire string, not a number
    assert(action.at("toPerp") == true);
    assert(action.at("nonce").is_number_integer());

    // User-signed action: its own nonce rides in the action and must match the
    // envelope nonce, and the payload must carry a signature.
    const auto& payload = exchange.lastPayload();
    assert(payload.at("nonce") == action.at("nonce"));
    assert(payload.contains("signature"));

    // postAction omits vaultAddress for usdClassTransfer.
    assert(!payload.contains("vaultAddress"));
}

void usdClassTransferToSpot() {
    TestExchange exchange;

    exchange.usdClassTransfer(1.0, /*to_perp=*/false);

    const auto& action = exchange.lastAction();
    assert(action.at("toPerp") == false);
    assert(action.at("amount") == "1");  // trailing zeros stripped
}

void usdClassTransferEncodesSubaccountInAmount() {
    const std::string vault = "0x1719884eb866cb12b2287399b15f7db5e7d775ea";
    TestExchange exchange(vault);

    exchange.usdClassTransfer(5.0, /*to_perp=*/true);

    // With a vault/subaccount configured, the address is appended to the
    // signed amount string (Python SDK parity), not sent as vaultAddress.
    const auto& action = exchange.lastAction();
    assert(action.at("amount") == "5 subaccount:" + vault);
    assert(!exchange.lastPayload().contains("vaultAddress"));
}

} // namespace

int main() {
    usdClassTransferSignatureMatchesPythonSdk();
    usdClassTransferPostsExpectedAction();
    usdClassTransferToSpot();
    usdClassTransferEncodesSubaccountInAmount();

    return 0;
}
