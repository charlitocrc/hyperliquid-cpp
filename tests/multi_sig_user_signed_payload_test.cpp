#include "multi_sig_fixture.hpp"

namespace {

void multiSigUserSignedPayloadSignatureMatchesPythonSdk() {
    auto wallet = Wallet::fromPrivateKey(SIGNER_KEY);

    nlohmann::json inner = {
        {"type", "sendAsset"},
        {"signatureChainId", "0x66eee"},
        {"hyperliquidChain", "Mainnet"},
        {"destination", "0x5e9ee1089755c3435139848e47e6635505d5a13a"},
        {"sourceDex", ""},
        {"destinationDex", "spot"},
        {"token", "USDC:0x8f254b963e8468305d409b33aa137c67"},
        {"amount", "12.5"},
        {"fromSubAccount", ""},
        {"nonce", NONCE}
    };

    auto sig = hyperliquid::signMultiSigUserSignedActionPayload(
        *wallet, inner, sendAssetSignTypes(), "HyperliquidTransaction:SendAsset",
        MULTI_SIG_USER, LEADER, /*is_mainnet=*/true).toJson();

    assert(sig.at("r") == "0xe7837b7c8680e93a91fcdb6d1c17011014bcd547752d3baee828f709c4e2b54d");
    assert(sig.at("s") == "0x1d346b635a4390257bb273c422acf0548125cd09c48997f6ec32a64b1d2a9489");
    assert(sig.at("v") == 27);

    // Signing must not mutate the caller's action -- multiSig() sends the same
    // object, and an extra field there would change the hash the leader signs.
    assert(!inner.contains("payloadMultiSigUser"));
    assert(!inner.contains("outerSigner"));
}

} // namespace

int main() {
    multiSigUserSignedPayloadSignatureMatchesPythonSdk();
    return 0;
}
