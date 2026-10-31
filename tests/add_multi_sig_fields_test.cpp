#include "multi_sig_fixture.hpp"

namespace {

void addMultiSigFieldsLowercasesAndKeepsAction() {
    nlohmann::json action = {{"type", "sendAsset"}, {"amount", "12.5"}};

    auto enriched = hyperliquid::addMultiSigFields(
        action, MULTI_SIG_USER, "0x14791697260E4c9A71f18484C9f997B308e59325");

    assert(enriched.at("payloadMultiSigUser") == MULTI_SIG_USER);
    assert(enriched.at("outerSigner") == LEADER);
    assert(enriched.at("amount") == "12.5");
    // The caller's action is untouched, so it can still be sent as-is.
    assert(!action.contains("outerSigner"));
}

} // namespace

int main() {
    addMultiSigFieldsLowercasesAndKeepsAction();
    return 0;
}
