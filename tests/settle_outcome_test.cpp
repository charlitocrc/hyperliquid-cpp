#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.settleOutcome("ab", {7, 1,
                                  {"template:abc", "expiry:20260801-0600|target:100|underlying:ABC"},
                                  {"template:Over", "template:Under"}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "settleOutcome":{"outcome":7,"settleFraction":"1","details":"",
            "nameAndDescription":["template:abc","expiry:20260801-0600|target:100|underlying:ABC"],
            "sideNames":["template:Over","template:Under"]}}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.settleOutcome("", {}); }));
    assert(exchange.requests().size() == sent);
}
