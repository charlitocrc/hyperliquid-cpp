#include "test_exchange.hpp"

#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

constexpr const char* VALIDATOR = "0xb8f45222a3246a2b0104696a1df26842007c5bc5";
constexpr uint64_t ONE_HYPE = 100000000;  // HYPE has 8 decimals

// tokenDelegate is pinned against the Python SDK, which implements it:
//   sign_token_delegate_action(wallet,
//       {"type": "tokenDelegate", "validator": VALIDATOR, "wei": 100000000,
//        "isUndelegate": False, "nonce": 1583838}, is_mainnet=True)
void tokenDelegateSignatureMatchesPythonSdk() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::json action = {
        {"type", "tokenDelegate"},
        {"validator", VALIDATOR},
        {"wei", ONE_HYPE},
        {"isUndelegate", false},
        {"nonce", 1583838}
    };

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"validator", "address"},
        {"wei", "uint64"},
        {"isUndelegate", "bool"},
        {"nonce", "uint64"}
    };

    auto sig = hyperliquid::signUserSignedAction(
        *wallet, action, payload_types,
        "HyperliquidTransaction:TokenDelegate", /*is_mainnet=*/true);

    const auto json = sig.toJson();
    assert(json.at("r") == "0xec05cea1ef6651967651e74358beb7e81db63c5bd2b2b964a1196382d775488e");
    assert(json.at("s") == "0x0854284ca873049f0b38e641dbcf425bc20ac87399cca2c5367857d412ad2ace");
    assert(json.at("v") == 27);
}

// The Python SDK has no cDeposit/cWithdraw, and the docs publish the action
// body but not the EIP-712 type list, so these two are pinned against
// eth_account driven with the type list inferred from that body:
//   sign_user_signed_action(wallet, {"type": "cDeposit", "wei": 100000000,
//       "nonce": 1583838},
//       [hyperliquidChain string, wei uint64, nonce uint64],
//       "HyperliquidTransaction:CDeposit", is_mainnet=True)
// That cross-checks the C++ EIP-712 encoding (notably uint64 wei) against a
// reference implementation. It cannot confirm the primary type string itself,
// which the exchange is the only authority on -- if these ever fail live with
// "does not exist", the type name is the first thing to suspect.
void stakingBalanceSignaturesMatchEthAccount() {
    auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    std::vector<hyperliquid::EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"wei", "uint64"},
        {"nonce", "uint64"}
    };

    nlohmann::json deposit = {
        {"type", "cDeposit"}, {"wei", ONE_HYPE}, {"nonce", 1583838}};
    auto deposit_sig = hyperliquid::signUserSignedAction(
        *wallet, deposit, payload_types,
        "HyperliquidTransaction:CDeposit", /*is_mainnet=*/true).toJson();
    assert(deposit_sig.at("r") == "0xdd1352cd82455f8231a1914db0ad672cb5f2e05021e69bfb76fb965c55c055c3");
    assert(deposit_sig.at("s") == "0x12ad67cfff4924b19badfc8714df90090722db848ab0f33c7d515d6d615c1132");
    assert(deposit_sig.at("v") == 27);

    nlohmann::json withdraw = {
        {"type", "cWithdraw"}, {"wei", ONE_HYPE}, {"nonce", 1583838}};
    auto withdraw_sig = hyperliquid::signUserSignedAction(
        *wallet, withdraw, payload_types,
        "HyperliquidTransaction:CWithdraw", /*is_mainnet=*/true).toJson();
    assert(withdraw_sig.at("r") == "0x2e689a729096ec76194af44d430300e4f14cb1d35f3a3c1dc9bf633ef43c72bd");
    assert(withdraw_sig.at("s") == "0x34987338c63c01806b039a48e52443b8a098c87b30462aba8fe3d00eb7a4dcdb");
    assert(withdraw_sig.at("v") == 28);
}

void stakingActionsPostExpectedWire() {
    TestExchange exchange;

    exchange.cDeposit(ONE_HYPE);
    exchange.cWithdraw(250000000);
    exchange.tokenDelegate(VALIDATOR, ONE_HYPE, /*is_undelegate=*/true);

    assert(exchange.requests().size() == 3);

    const auto& deposit = exchange.requests()[0].payload.at("action");
    assert(exchange.requests()[0].path == "/exchange");
    assert(deposit.at("type") == "cDeposit");
    assert(deposit.at("wei") == ONE_HYPE);
    assert(deposit.at("wei").is_number());  // a number on the wire, not a string
    assert(deposit.at("nonce").is_number_integer());

    const auto& withdraw = exchange.requests()[1].payload.at("action");
    assert(withdraw.at("type") == "cWithdraw");
    assert(withdraw.at("wei") == 250000000);

    const auto& delegate = exchange.requests()[2].payload.at("action");
    assert(delegate.at("type") == "tokenDelegate");
    assert(delegate.at("validator") == VALIDATOR);
    assert(delegate.at("wei") == ONE_HYPE);
    assert(delegate.at("isUndelegate") == true);

    // User-signed: the action nonce is the envelope nonce. The envelope still
    // carries a vaultAddress field (null here, with no vault configured), like
    // usdSend and every other user-signed action -- unlike usdClassTransfer
    // and sendAsset, which omit the field entirely. A vault is not part of an
    // EIP-712 payload, so there is nothing here for the signature to cover.
    for (const auto& request : exchange.requests()) {
        assert(request.payload.at("nonce") == request.payload.at("action").at("nonce"));
        assert(request.payload.contains("signature"));
        assert(request.payload.at("vaultAddress").is_null());
    }
}

// The validator is an EIP-712 address field, so a mixed-case address hashes
// the same -- but the exchange rebuilds the payload from what it receives, and
// the docs recommend lowercasing before signing either way.
void tokenDelegateLowercasesValidator() {
    TestExchange exchange;

    exchange.tokenDelegate("0xB8F45222A3246A2B0104696A1DF26842007C5BC5", ONE_HYPE, false);

    assert(exchange.lastAction().at("validator") == VALIDATOR);
}

void stakingActionsRejectBadInput() {
    TestExchange exchange;

    // Zero wei is a no-op the exchange refuses; catching it here is usually a
    // units mistake (HYPE passed where wei was wanted).
    bool threw = false;
    try {
        exchange.cDeposit(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        exchange.cWithdraw(0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        exchange.tokenDelegate(VALIDATOR, 0, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        exchange.tokenDelegate("0xnope", ONE_HYPE, false);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    assert(exchange.requests().empty());  // nothing was sent
}

} // namespace

int main() {
    tokenDelegateSignatureMatchesPythonSdk();
    stakingBalanceSignaturesMatchEthAccount();
    stakingActionsPostExpectedWire();
    tokenDelegateLowercasesValidator();
    stakingActionsRejectBadInput();

    return 0;
}
