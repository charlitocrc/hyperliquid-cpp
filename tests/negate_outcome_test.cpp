#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.negateOutcome(3, 11, 4);
    assertSent(exchange,
               R"({"type":"userOutcome","negateOutcome":{"question":3,"outcome":11,"amount":"4"}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.negateOutcome(3, 11, 0); }));
    assert(exchange.requests().size() == sent);
}
