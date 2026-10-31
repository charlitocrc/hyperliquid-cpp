#include "l1_action_signing_fixture.hpp"

namespace {

void cancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.cancel("BTC", 12345);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 12345;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    cancelSignsExpectedAction();
    return 0;
}
