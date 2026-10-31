#include "l1_action_signing_fixture.hpp"

namespace {

void twapOrderSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapOrder("BTC", true, 1.5, 30);

    nlohmann::ordered_json twap;
    twap["a"] = 0;
    twap["b"] = true;
    twap["s"] = "1.5";
    twap["r"] = false;
    twap["m"] = 30;
    twap["t"] = false;

    nlohmann::ordered_json action;
    action["type"] = "twapOrder";
    action["twap"] = twap;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    twapOrderSignsExpectedAction();
    return 0;
}
