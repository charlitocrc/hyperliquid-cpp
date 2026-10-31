#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.mergeOutcome(7, 2.5);
    assertSent(exchange, R"({"type":"userOutcome","mergeOutcome":{"outcome":7,"amount":"2.5"}})");

    exchange.mergeOutcome(7);
    assertSent(exchange, R"({"type":"userOutcome","mergeOutcome":{"outcome":7,"amount":null}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.mergeOutcome(7, -1.0); }));
    assert(exchange.requests().size() == sent);
}
