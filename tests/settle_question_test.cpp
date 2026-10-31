#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.settleQuestion("ab", 3,
                            {{11, 1, {"template:abc-outcome", "choice:A"}, {"Yes", "No"}},
                             {12, 0, {"template:abc-outcome", "choice:B"}, {"Yes", "No"}}},
                            {"template:abc", "expiry:20260801-1830"});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "settleQuestion2":{"question":3,"outcomeSettlements":[
            {"outcome":11,"settleFraction":"1","details":"",
             "nameAndDescription":["template:abc-outcome","choice:A"],"sideNames":["Yes","No"]},
            {"outcome":12,"settleFraction":"0","details":"",
             "nameAndDescription":["template:abc-outcome","choice:B"],"sideNames":["Yes","No"]}],
            "nameAndDescription":["template:abc","expiry:20260801-1830"]}}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.settleQuestion("ab", 3, {}, {"q", "d"}); }));
    assert(exchange.requests().size() == sent);
}
