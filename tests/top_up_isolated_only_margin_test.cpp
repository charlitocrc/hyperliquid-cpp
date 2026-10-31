#include "rate_limits_margin_fixture.hpp"

namespace {

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

} // namespace

int main() {
    topUpPostsExpectedAction();
    topUpEncodesFractionalLeverageAsFloatString();
    topUpRejectsNonPositiveLeverage();
    return 0;
}
