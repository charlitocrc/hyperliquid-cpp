#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.deactivateOutcomeDeployer();
    assertSent(exchange, R"({"type":"activateOutcomeDeployer","deactivate":null})");

}
