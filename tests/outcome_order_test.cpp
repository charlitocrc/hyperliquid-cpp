#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    hyperliquid::OrderType limit;
    limit.limit = hyperliquid::LimitOrderType{"Gtc"};

    exchange.order("#142000", true, 10.6, 0.2859349, limit);
    auto wire = exchange.lastAction().at("orders").at(0);
    assert(wire.at("a") == 100142000);
    assert(wire.at("p") == "0.28593");
    assert(wire.at("s") == "11");

    // The generic spot rule would allow 0.012346 here.
    exchange.order("#142001", false, 3, 0.0123456, limit);
    assert(exchange.lastAction().at("orders").at(0).at("p") == "0.01235");
}
