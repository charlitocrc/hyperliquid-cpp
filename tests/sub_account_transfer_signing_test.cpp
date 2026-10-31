#include "l1_action_signing_fixture.hpp"

namespace {

void subAccountTransferIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.subAccountTransfer(TEST_VAULT, true, 1000000);

    nlohmann::ordered_json action;
    action["type"] = "subAccountTransfer";
    action["subAccountUser"] = TEST_VAULT;
    action["isDeposit"] = true;
    action["usd"] = 1000000;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    subAccountTransferIgnoresConfiguredVault();
    return 0;
}
