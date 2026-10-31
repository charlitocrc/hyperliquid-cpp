#include "l1_action_signing_fixture.hpp"

namespace {

void updateLeverageSignsExpectedAction() {
    TestExchange exchange;

    exchange.updateLeverage(10, "BTC", true);

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = 0;
    action["isCross"] = true;
    action["leverage"] = 10;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    updateLeverageSignsExpectedAction();
    return 0;
}
