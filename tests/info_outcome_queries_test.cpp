#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

void checkOutcomeMeta() {
    {
        TestInfo info;
        info.outcomeMeta();
        assertSingleRequest(info, {{"type", "outcomeMeta"}});
    }
}

void checkOutcomeTemplates() {
    {
        TestInfo info;
        info.outcomeTemplates();
        assertSingleRequest(info, {{"type", "outcomeTemplates"}});
    }
}

void checkOutcomeDeployerLimits() {
    {
        TestInfo info;
        info.outcomeDeployerLimits("out");
        assertSingleRequest(info, {{"type", "outcomeDeployerLimits"}, {"venue", "out"}});
    }
    {
        // An empty venue is an argument error, not a request.
        TestInfo info;
        bool threw = false;
        try {
            info.outcomeDeployerLimits("");
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw && info.requests().empty());
    }
}

void checkSettledOutcome() {
    {
        TestInfo info;
        info.settledOutcome(1113);
        assertSingleRequest(info, {{"type", "settledOutcome"}, {"outcome", 1113}});
        assert(info.requests()[0].payload.at("outcome").is_number_integer());
    }
    {
    TestInfo info;
    info.setResponse(nullptr);

    const auto response = info.settledOutcome(9999);

    assert(response.is_null());
    }
}

} // namespace

int main() {
    checkOutcomeMeta();
    checkOutcomeTemplates();
    checkOutcomeDeployerLimits();
    checkSettledOutcome();
    return 0;
}
