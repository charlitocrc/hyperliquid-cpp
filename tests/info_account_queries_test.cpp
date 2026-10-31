#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void checkExtraAgents() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.extraAgents(USER);
        assertSingleRequest(info, {{"type", "extraAgents"}, {"user", USER}});
    }
}

void checkActiveAssetData() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
    TestInfo info;

    info.activeAssetData(USER, "ETH");

    assertSingleRequest(info, {{"type", "activeAssetData"}, {"user", USER}, {"coin", "ETH"}});
    }
}

} // namespace

int main() {
    checkExtraAgents();
    checkActiveAssetData();
    return 0;
}
