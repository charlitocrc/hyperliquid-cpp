#include "multi_sig_fixture.hpp"

namespace {

void multiSigActionSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);

    nlohmann::ordered_json inner_signature;
    inner_signature["r"] = "0x70c30c9dcb9ca1cc104997cba902eae08f6423771bfd6a15af800ac6786976eb";
    inner_signature["s"] = "0x4b9f4c4c961873cb98a5cc4d49662a93e5619e01693958e07e48789667d49c4b";
    inner_signature["v"] = 28;

    nlohmann::ordered_json payload;
    payload["multiSigUser"] = MULTI_SIG_USER;
    payload["outerSigner"] = LEADER;
    payload["action"] = innerOrderAction();

    nlohmann::ordered_json action;
    action["type"] = "multiSig";
    action["signatureChainId"] = "0x66eee";
    action["signatures"] = nlohmann::ordered_json::array({inner_signature});
    action["payload"] = payload;

    auto no_vault = hyperliquid::signMultiSigAction(
        *wallet, action, std::nullopt, NONCE, std::nullopt, /*is_mainnet=*/true).toJson();
    assert(no_vault.at("r") == "0xc219372eb83ebc8bb00d21d27958e10accb0829980d363eccf7bcf9fe547fa42");
    assert(no_vault.at("s") == "0x486eb6269a97ed05c1897a0c4b9bc5c046aa7aba3ce7b7aedcd498bef794b364");
    assert(no_vault.at("v") == 27);

    // The vault is part of the hashed preimage, not just the envelope.
    auto with_vault = hyperliquid::signMultiSigAction(
        *wallet, action, std::string(VAULT), NONCE, std::nullopt, /*is_mainnet=*/true).toJson();
    assert(with_vault.at("r") == "0xd3947040a2798bef0b2e6ad7d9b7dc43b1eeaa0da5ec79caf89c37c987f137e8");
    assert(with_vault.at("s") == "0x78b80886c2a139b002d2ae7a59274b3b04cdac3251154b861e63ca00165d6910");
    assert(with_vault.at("v") == 28);
}

} // namespace

int main() {
    multiSigActionSignatureMatchesPythonSdk();
    return 0;
}
