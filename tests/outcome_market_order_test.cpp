#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.marketOpen("#142000", true, 5, 0.98);  // 0.98 * 1.05 = 1.029
    assert(exchange.lastAction().at("orders").at(0).at("p") == "0.99999");
}
