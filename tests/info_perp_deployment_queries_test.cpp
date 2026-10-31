#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void perpDexLimitsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpDexLimits("xyz");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexLimits"}, {"dex", "xyz"}});
}

void perpDexLimitsRejectsEmptyDex() {
    TestInfo info;

    bool threw = false;
    try {
        info.perpDexLimits("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(info.requests().empty());
}

void perpDexStatusSendsEmptyDexForFirstDex() {
    TestInfo info;

    const auto response = info.perpDexStatus();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexStatus"}, {"dex", ""}});
}

void perpDexsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpDexs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexs"}});
}

void queryPerpDeployAuctionStatusUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.queryPerpDeployAuctionStatus();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDeployAuctionStatus"}});
}

} // namespace

int main() {
    perpDexLimitsUsesExpectedPayload();
    perpDexLimitsRejectsEmptyDex();
    perpDexStatusSendsEmptyDexForFirstDex();
    perpDexsUsesExpectedPayload();
    queryPerpDeployAuctionStatusUsesExpectedPayload();
    return 0;
}
