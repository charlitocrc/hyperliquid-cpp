#include "multi_sig_fixture.hpp"

namespace {

void addMultiSigTypesInsertsAfterHyperliquidChain() {
    auto enriched = hyperliquid::addMultiSigTypes(sendAssetSignTypes());

    assert(enriched.size() == sendAssetSignTypes().size() + 2);
    assert(enriched[0].name == "hyperliquidChain");
    assert(enriched[1].name == "payloadMultiSigUser");
    assert(enriched[1].type == "address");
    assert(enriched[2].name == "outerSigner");
    assert(enriched[2].type == "address");
    assert(enriched[3].name == "destination");
    assert(enriched.back().name == "nonce");
}

void addMultiSigTypesThrowsWithoutHyperliquidChain() {
    bool threw = false;
    try {
        hyperliquid::addMultiSigTypes({{"amount", "string"}, {"nonce", "uint64"}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    addMultiSigTypesInsertsAfterHyperliquidChain();
    addMultiSigTypesThrowsWithoutHyperliquidChain();
    return 0;
}
