#include "l1_action_signing_fixture.hpp"

namespace {

void createSubAccountIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.createSubAccount("desk-1");

    nlohmann::ordered_json action;
    action["type"] = "createSubAccount";
    action["name"] = "desk-1";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    createSubAccountIgnoresConfiguredVault();
    return 0;
}
