#include "rate_limits_margin_fixture.hpp"

namespace {

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
    reserveRequestWeightPostsExpectedAction();
    reserveRequestWeightRejectsNonPositiveWeight();
    return 0;
}
