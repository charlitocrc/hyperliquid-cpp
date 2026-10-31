#include "l1_action_signing_fixture.hpp"

namespace {

void twapCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapCancel("BTC", 77738308);

    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = 0;
    action["t"] = 77738308;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    twapCancelSignsExpectedAction();
    return 0;
}
