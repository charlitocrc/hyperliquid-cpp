#include "twap_orders_fixture.hpp"

namespace {

void twapCancelPostsExpectedAction() {
    TestExchange exchange;

    exchange.twapCancel("BTC", 77738308);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "twapCancel");
    assert(action.at("a") == 0);
    assert(action.at("t") == 77738308);
}

} // namespace

int main() {
    twapCancelPostsExpectedAction();
    return 0;
}
