#include "l1_action_signing_fixture.hpp"

namespace {

void scheduleCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.scheduleCancel(1700000000000);

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";
    action["time"] = 1700000000000;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void scheduleCancelWithoutTimeOmitsField() {
    TestExchange exchange;

    exchange.scheduleCancel();

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    scheduleCancelSignsExpectedAction();
    scheduleCancelWithoutTimeOmitsField();
    return 0;
}
