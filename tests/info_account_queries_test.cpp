// Cover for the info queries whose method name does not match the wire type,
// and for the ones taking a numeric parameter.
//
// These are one-line POST wrappers, so the only thing that can break is the
// request itself: a wrong type string, a wrong parameter name, or a parameter
// silently dropped. Four of the staking queries are named after what they
// return rather than what they ask for (userStakingSummary sends
// "delegatorSummary"), which is exactly the mapping worth pinning.

#include "test_info.hpp"

#include <cassert>
#include <string>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";

void parameterlessQueriesUseExpectedPayload() {
    {
        TestInfo info;
        info.allPerpMetas();
        assertSingleRequest(info, {{"type", "allPerpMetas"}});
    }
    {
        TestInfo info;
        info.spotPairDeployAuctionStatus();
        assertSingleRequest(info, {{"type", "spotPairDeployAuctionStatus"}});
    }
    {
        TestInfo info;
        info.outcomeMeta();
        assertSingleRequest(info, {{"type", "outcomeMeta"}});
    }
    {
        TestInfo info;
        info.allBorrowLendReserveStates();
        assertSingleRequest(info, {{"type", "allBorrowLendReserveStates"}});
    }
}

// The method names deliberately describe the answer; the wire types are the
// exchange's names for the questions. Swapping any two would still compile.
void stakingQueriesSendTheirOwnWireType() {
    {
        TestInfo info;
        info.userStakingSummary(USER);
        assertSingleRequest(info, {{"type", "delegatorSummary"}, {"user", USER}});
    }
    {
        TestInfo info;
        info.userStakingDelegations(USER);
        assertSingleRequest(info, {{"type", "delegations"}, {"user", USER}});
    }
    {
        TestInfo info;
        info.userStakingRewards(USER);
        assertSingleRequest(info, {{"type", "delegatorRewards"}, {"user", USER}});
    }
    {
        TestInfo info;
        info.delegatorHistory(USER);
        assertSingleRequest(info, {{"type", "delegatorHistory"}, {"user", USER}});
    }
}

void userQueriesUseExpectedPayload() {
    {
        TestInfo info;
        info.spotDeployState(USER);
        assertSingleRequest(info, {{"type", "spotDeployState"}, {"user", USER}});
    }
    {
        TestInfo info;
        info.borrowLendUserState(USER);
        assertSingleRequest(info, {{"type", "borrowLendUserState"}, {"user", USER}});
    }
    {
        TestInfo info;
        info.extraAgents(USER);
        assertSingleRequest(info, {{"type", "extraAgents"}, {"user", USER}});
    }
}

void activeAssetDataSendsUserAndCoin() {
    TestInfo info;

    info.activeAssetData(USER, "ETH");

    assertSingleRequest(info, {{"type", "activeAssetData"}, {"user", USER}, {"coin", "ETH"}});
}

// Numeric parameters must go out as JSON numbers, not strings: the exchange
// rejects the string form outright.
void numericQueriesSendNumbers() {
    {
        TestInfo info;
        info.settledOutcome(1113);
        assertSingleRequest(info, {{"type", "settledOutcome"}, {"outcome", 1113}});
        assert(info.requests()[0].payload.at("outcome").is_number_integer());
    }
    {
        TestInfo info;
        info.borrowLendReserveState(0);
        assertSingleRequest(info, {{"type", "borrowLendReserveState"}, {"token", 0}});
        assert(info.requests()[0].payload.at("token").is_number_integer());
    }
}

// Responses are handed back untouched, per CLAUDE.md section 5 -- including a
// bare null, which settledOutcome returns for an outcome that has not settled.
void responsesArePassedThroughUnchanged() {
    TestInfo info;
    info.setResponse(nullptr);

    const auto response = info.settledOutcome(9999);

    assert(response.is_null());
}

} // namespace

int main() {
    parameterlessQueriesUseExpectedPayload();
    stakingQueriesSendTheirOwnWireType();
    userQueriesUseExpectedPayload();
    activeAssetDataSendsUserAndCoin();
    numericQueriesSendNumbers();
    responsesArePassedThroughUnchanged();

    return 0;
}
