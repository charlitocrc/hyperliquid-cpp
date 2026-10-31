#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.splitOutcome(7, 123);
    assertSent(exchange, R"({"type":"userOutcome","splitOutcome":{"outcome":7,"amount":"123"}})");

    // A configured subaccount trades its own outcome balances, as with orders.
    const std::string vault = "0x1234567890abcdef1234567890abcdef12345678";
    TestExchange sub(vault);
    sub.splitOutcome(7, 1);
    assert(sub.lastPayload().at("vaultAddress") == vault);
    assertSent(sub, R"({"type":"userOutcome","splitOutcome":{"outcome":7,"amount":"1"}})", true, vault);

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.splitOutcome(7, 0); }));
    assert(exchange.requests().size() == sent);
}
