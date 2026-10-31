#include "multi_sig_fixture.hpp"

namespace {

void multiSigL1PayloadSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(SIGNER_KEY);

    auto sig = hyperliquid::signMultiSigL1ActionPayload(
        *wallet, innerOrderAction(), std::nullopt, NONCE, std::nullopt,
        MULTI_SIG_USER, LEADER, /*is_mainnet=*/true).toJson();

    assert(sig.at("r") == "0x70c30c9dcb9ca1cc104997cba902eae08f6423771bfd6a15af800ac6786976eb");
    assert(sig.at("s") == "0x4b9f4c4c961873cb98a5cc4d49662a93e5619e01693958e07e48789667d49c4b");
    assert(sig.at("v") == 28);
}

} // namespace

int main() {
    multiSigL1PayloadSignatureMatchesPythonSdk();
    return 0;
}
