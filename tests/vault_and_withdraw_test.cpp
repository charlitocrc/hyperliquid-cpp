// Pins the two actions that move money off the master account: vaultTransfer
// and withdraw3.
//
// Both are money paths with a silent failure mode. vaultTransfer encodes its
// amount as an integer with 6 decimals, so a missing or doubled scaling is a
// factor of a million, and it names its target vault inside the action, so a
// vault configured on the Exchange must not also ride in the envelope -- a
// signature that does not cover the vault it is sent with cannot be verified.
// withdraw3 sends USDC to an Arbitrum address that cannot be taken back.
//
// Reference signatures come from the Python SDK run against the same key and
// action, so a change to the EIP-712 or msgpack encoding fails here rather than
// at the exchange.

#include "test_exchange.hpp"

#include "hyperliquid/utils/conversions.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using hyperliquid::Wallet;
using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

constexpr const char* HLP_VAULT = "0xdfc24b077bc1425ad1dea75bcb6f8158e10df303";
constexpr const char* DESTINATION = "0x1234567890123456789012345678901234567890";

// 5 USD must reach the wire as 5000000, matching the Python SDK example that
// passes 5_000_000 for the same transfer.
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

// The action names its own vault, so a vault configured on the Exchange must
// appear in neither the envelope nor the signature.
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

// Pinned to the Python SDK:
//   sign_l1_action(wallet, {"type": "vaultTransfer",
//       "vaultAddress": "0xdfc2..f303", "isDeposit": True, "usd": 5000000},
//       None, 1583838, None, True)
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

// Pinned to the Python SDK:
//   sign_withdraw_from_bridge_action(wallet,
//       {"destination": "0x1234..7890", "amount": "25.5", "time": 1583838,
//        "type": "withdraw3"}, is_mainnet=True)
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

// Pins what a configured vault does to a withdrawal, which is not the same as
// for vaultTransfer above.
//
// withdraw3 is user-signed: the EIP-712 payload is {hyperliquidChain,
// destination, amount, time} and never covers a vault, so there is no signature
// mismatch to avoid and the action is identical either way. The envelope still
// carries the configured vault, matching the Python SDK.
//
// Whether the exchange then reads it is unverified -- a vault cannot withdraw
// to Arbitrum, so it is most likely ignored. Left at Python parity rather than
// changed on a guess; see TODO.md. Until that is settled, withdraw from an
// Exchange with no vault configured.
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
    vaultTransferScalesUsdToSixDecimals();
    vaultTransferIgnoresConfiguredVault();
    vaultTransferSignatureMatchesPythonSdk();
    vaultTransferRejectsNonPositiveAmounts();
    withdrawPostsExpectedAction();
    withdrawSignatureMatchesPythonSdk();
    withdrawCarriesConfiguredVaultInEnvelopeOnly();

    return 0;
}
