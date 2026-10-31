#include "l1_action_signing_fixture.hpp"

namespace {

void subAccountSpotTransferIgnoresConfiguredVault() {
    TestExchange exchange(TEST_VAULT);

    exchange.subAccountSpotTransfer(TEST_VAULT, false, "PURR:0x1", 1.5);

    nlohmann::ordered_json action;
    action["type"] = "subAccountSpotTransfer";
    action["subAccountUser"] = TEST_VAULT;
    action["isDeposit"] = false;
    action["token"] = "PURR:0x1";
    action["amount"] = "1.5";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

} // namespace

int main() {
    subAccountSpotTransferIgnoresConfiguredVault();
    return 0;
}
