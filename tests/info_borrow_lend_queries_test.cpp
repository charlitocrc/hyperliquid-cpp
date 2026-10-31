#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void checkAllBorrowLendReserveStates() {
    {
        TestInfo info;
        info.allBorrowLendReserveStates();
        assertSingleRequest(info, {{"type", "allBorrowLendReserveStates"}});
    }
}

void checkBorrowLendUserState() {
    constexpr const char* USER = "0x000000000056f99d36b6f2e0c51fd41496bbacb8";
    {
        TestInfo info;
        info.borrowLendUserState(USER);
        assertSingleRequest(info, {{"type", "borrowLendUserState"}, {"user", USER}});
    }
}

void checkBorrowLendReserveState() {
    {
        TestInfo info;
        info.borrowLendReserveState(0);
        assertSingleRequest(info, {{"type", "borrowLendReserveState"}, {"token", 0}});
        assert(info.requests()[0].payload.at("token").is_number_integer());
    }
}

} // namespace

int main() {
    checkAllBorrowLendReserveStates();
    checkBorrowLendUserState();
    checkBorrowLendReserveState();
    return 0;
}
