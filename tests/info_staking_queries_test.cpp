#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void checkUserStakingSummary() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.userStakingSummary(USER);
        assertSingleRequest(info, {{"type", "delegatorSummary"}, {"user", USER}});
    }
}

void checkUserStakingDelegations() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.userStakingDelegations(USER);
        assertSingleRequest(info, {{"type", "delegations"}, {"user", USER}});
    }
}

void checkUserStakingRewards() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.userStakingRewards(USER);
        assertSingleRequest(info, {{"type", "delegatorRewards"}, {"user", USER}});
    }
}

void checkDelegatorHistory() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.delegatorHistory(USER);
        assertSingleRequest(info, {{"type", "delegatorHistory"}, {"user", USER}});
    }
}

} // namespace

int main() {
    checkUserStakingSummary();
    checkUserStakingDelegations();
    checkUserStakingRewards();
    checkDelegatorHistory();
    return 0;
}
