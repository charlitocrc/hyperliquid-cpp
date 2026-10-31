#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void tokenDetailsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.tokenDetails("0xc1fb593aeffbeb02f85e0308e9956a90");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "tokenDetails"},
                               {"tokenId", "0xc1fb593aeffbeb02f85e0308e9956a90"}});
}

void spotMetaAndAssetCtxsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.spotMetaAndAssetCtxs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "spotMetaAndAssetCtxs"}});
}

void checkSpotPairDeployAuctionStatus() {
    {
        TestInfo info;
        info.spotPairDeployAuctionStatus();
        assertSingleRequest(info, {{"type", "spotPairDeployAuctionStatus"}});
    }
}

void checkSpotDeployState() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.spotDeployState(USER);
        assertSingleRequest(info, {{"type", "spotDeployState"}, {"user", USER}});
    }
}

} // namespace

int main() {
    tokenDetailsUsesExpectedPayload();
    spotMetaAndAssetCtxsUsesExpectedPayload();
    checkSpotPairDeployAuctionStatus();
    checkSpotDeployState();
    return 0;
}
