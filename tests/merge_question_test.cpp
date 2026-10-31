#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange;
    exchange.mergeQuestion(3);
    assertSent(exchange, R"({"type":"userOutcome","mergeQuestion":{"question":3,"amount":null}})");

}
