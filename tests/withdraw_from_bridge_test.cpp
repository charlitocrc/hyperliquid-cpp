#include "vault_and_withdraw_fixture.hpp"

namespace {

void withdrawPostsExpectedAction() {
    TestExchange exchange;

    exchange.withdrawFromBridge(25.5, DESTINATION);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "withdraw3");
    assert(action.at("destination") == DESTINATION);
    assert(action.at("amount") == "25.5");  // floatToWire string, not a number
    assert(action.at("time").is_number_integer());

    // User-signed action: the nonce is the action's own "time", so the two
    // cannot drift apart.
    assert(exchange.lastPayload().at("nonce") == action.at("time"));
}

void withdrawSignatureMatchesPythonSdk() {
    nlohmann::json action = {
        {"type", "withdraw3"},
        {"destination", DESTINATION},
        {"amount", "25.5"},
        {"time", 1583838}
    };

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"amount", "string"},
        {"time", "uint64"}
    };

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto pinned = hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:Withdraw", /*is_mainnet=*/true).toJson();

    assert(pinned.at("r") == "0xb24dcaced2a3e35c0c513fa8d731b3d93d8cb807cf2b568eda4beaf53acae7d5");
    assert(pinned.at("s") == "0x4271dcaf4a70bfc02c1fe104994b1e6d3e5452130de004e707e3f75b524aaadc");
    assert(pinned.at("v") == 28);
}

void withdrawCarriesConfiguredVaultInEnvelopeOnly() {
    TestExchange plain;
    plain.withdrawFromBridge(25.5, DESTINATION);

    TestExchange with_vault("0x1234567890abcdef1234567890abcdef12345678");
    with_vault.withdrawFromBridge(25.5, DESTINATION);

    // The signed action is what matters, and a vault does not enter it.
    const auto& plain_action = plain.lastAction();
    const auto& vault_action = with_vault.lastAction();
    assert(!plain_action.contains("vaultAddress"));
    assert(!vault_action.contains("vaultAddress"));
    assert(plain_action.at("destination") == vault_action.at("destination"));
    assert(plain_action.at("amount") == vault_action.at("amount"));

    // Envelope only, and only when one is configured.
    assert(plain.lastPayload().at("vaultAddress").is_null());
    assert(with_vault.lastPayload().at("vaultAddress") ==
           "0x1234567890abcdef1234567890abcdef12345678");
}

} // namespace

int main() {
    withdrawPostsExpectedAction();
    withdrawSignatureMatchesPythonSdk();
    withdrawCarriesConfiguredVaultInEnvelopeOnly();
    return 0;
}
