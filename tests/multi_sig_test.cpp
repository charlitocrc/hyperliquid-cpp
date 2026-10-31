#include "multi_sig_fixture.hpp"

namespace {

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
    multiSigPostsExpectedActionAndSignature();
    multiSigSignsAndSendsConfiguredVault();
    multiSigRejectsUnusableInput();
    return 0;
}
