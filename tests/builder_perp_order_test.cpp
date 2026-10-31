#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.info_.registerPerpMeta(hyperliquid::Meta{{hyperliquid::AssetInfo{"test:ABC", 0}}}, 110000);
    hyperliquid::OrderType limit;
    limit.limit = hyperliquid::LimitOrderType{"Gtc"};

    exchange.order("test:ABC", true, 1, 0.0012345678, limit);
    auto wire = exchange.lastAction().at("orders").at(0);
    assert(wire.at("a") == 110000);
    assert(wire.at("p") == "0.001235");  // spot's 8 decimals would send 0.0012346
}
