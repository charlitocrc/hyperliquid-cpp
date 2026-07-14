#include "test_exchange.hpp"

#include <cassert>
#include <stdexcept>

using hyperliquid_test::TestExchange;

namespace {

// The action hash is keccak(msgpack(action)), so key order in the serialized
// action is load-bearing. Assert the literal serialization, not just json
// equality (which ignores order).
void topUpPostsExpectedAction() {
    TestExchange exchange;

    exchange.topUpIsolatedOnlyMargin("ETH", 5.0);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "topUpIsolatedOnlyMargin");
    assert(action.at("asset") == 1);  // ETH is index 1 in the fixture meta
    assert(action.at("leverage") == "5");  // float string, trailing zeros stripped
}

void topUpEncodesFractionalLeverageAsFloatString() {
    TestExchange exchange;

    exchange.topUpIsolatedOnlyMargin("BTC", 2.5);

    const auto& action = exchange.lastAction();
    assert(action.at("asset") == 0);
    assert(action.at("leverage") == "2.5");
}

void topUpRejectsNonPositiveLeverage() {
    TestExchange exchange;

    for (double bad : {0.0, -3.0}) {
        bool threw = false;
        try {
            exchange.topUpIsolatedOnlyMargin("ETH", bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }
    assert(exchange.requests().empty());  // nothing was sent
}

void reserveRequestWeightPostsExpectedAction() {
    TestExchange exchange;

    exchange.reserveRequestWeight(10);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "reserveRequestWeight");
    assert(action.at("weight") == 10);

    // Signing of the envelope is covered by l1_action_signing_test.
}

void reserveRequestWeightRejectsNonPositiveWeight() {
    TestExchange exchange;

    for (int64_t bad : {int64_t{0}, int64_t{-1}}) {
        bool threw = false;
        try {
            exchange.reserveRequestWeight(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }
    assert(exchange.requests().empty());
}

} // namespace

int main() {
    topUpPostsExpectedAction();
    topUpEncodesFractionalLeverageAsFloatString();
    topUpRejectsNonPositiveLeverage();
    reserveRequestWeightPostsExpectedAction();
    reserveRequestWeightRejectsNonPositiveWeight();

    return 0;
}
