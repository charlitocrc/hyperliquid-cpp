#include "l1_action_signing_fixture.hpp"

namespace {

void setReferrerIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.setReferrer("HYPERLIQUID");

    nlohmann::ordered_json action;
    action["type"] = "setReferrer";
    action["code"] = "HYPERLIQUID";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    setReferrerIgnoresConfiguredVault();
    return 0;
}
