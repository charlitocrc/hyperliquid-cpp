#include "vault_and_withdraw_fixture.hpp"

namespace {

void vaultTransferScalesUsdToSixDecimals() {
    TestExchange exchange;

    exchange.vaultTransfer(HLP_VAULT, /*is_deposit=*/true, 5.0);

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "vaultTransfer");
    assert(action.at("vaultAddress") == HLP_VAULT);
    assert(action.at("isDeposit") == true);
    assert(action.at("usd") == 5000000);
    assert(action.at("usd").is_number_integer() && "usd is an int, not a float string");

    // Fractional dollars survive the scaling; sub-cent amounts round.
    exchange.vaultTransfer(HLP_VAULT, /*is_deposit=*/false, 12.34);
    assert(exchange.lastAction().at("usd") == 12340000);
    assert(exchange.lastAction().at("isDeposit") == false);
}

void vaultTransferIgnoresConfiguredVault() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");

    exchange.vaultTransfer(HLP_VAULT, /*is_deposit=*/true, 5.0);

    const auto& payload = exchange.lastPayload();
    assert(payload.at("vaultAddress").is_null() &&
           "a configured vault must not ride along with vaultTransfer");
    assert(payload.at("action").at("vaultAddress") == HLP_VAULT);

    // And the signature must be the one made with no vault, or the exchange
    // cannot verify what it received.
    nlohmann::ordered_json action;
    action["type"] = "vaultTransfer";
    action["vaultAddress"] = HLP_VAULT;
    action["isDeposit"] = true;
    action["usd"] = 5000000;

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto expected = hyperliquid::signL1Action(
        *wallet, action, std::nullopt,
        payload.at("nonce").get<int64_t>(), std::nullopt, /*is_mainnet=*/false);
    assert(payload.at("signature") == expected.toJson());
}

void vaultTransferSignatureMatchesPythonSdk() {
    nlohmann::ordered_json action;
    action["type"] = "vaultTransfer";
    action["vaultAddress"] = HLP_VAULT;
    action["isDeposit"] = true;
    action["usd"] = 5000000;

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto pinned = hyperliquid::signL1Action(
        *wallet, action, std::nullopt, 1583838, std::nullopt, /*is_mainnet=*/true).toJson();

    assert(pinned.at("r") == "0x4423480f11bf14b22b1cfe32d10f2c135ec0d2246bbd63a946b16f22e68cc80e");
    assert(pinned.at("s") == "0x0b3a52c2ac049f89061af804a801ebfc4520e967fb1dd2f41ef2a4cd017f6dc9");
    assert(pinned.at("v") == 28);
}

void vaultTransferRejectsNonPositiveAmounts() {
    TestExchange exchange;
    for (double bad : {0.0, -5.0}) {
        bool threw = false;
        try {
            exchange.vaultTransfer(HLP_VAULT, /*is_deposit=*/true, bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw && "a non-positive transfer must not reach the exchange");
    }
    assert(exchange.requests().empty());
}

} // namespace

int main() {
    vaultTransferScalesUsdToSixDecimals();
    vaultTransferIgnoresConfiguredVault();
    vaultTransferSignatureMatchesPythonSdk();
    vaultTransferRejectsNonPositiveAmounts();
    return 0;
}
