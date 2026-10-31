#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.activateOutcomeDeployer("ab");
    assertSent(exchange, R"({"type":"activateOutcomeDeployer","activate":{"venueName":"ab"}})");
    assert(exchange.lastPayload().at("vaultAddress").is_null());

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.activateOutcomeDeployer(""); }));
    assert(exchange.requests().size() == sent);
}
