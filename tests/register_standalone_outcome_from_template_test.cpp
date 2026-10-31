#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    // Keywords given out of order go out sorted.
    hyperliquid::OutcomeTemplateInstance standalone{
        "abc", {{"underlying", "ABC"}, {"expiry", "20260801-0600"}, {"target", "100"}}};
    exchange.registerStandaloneOutcomeFromTemplate("ab", standalone, 1);
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "registerStandaloneOutcomeFromTemplate":{"id":"abc",
            "keywordToValue":[["expiry","20260801-0600"],["target","100"],["underlying","ABC"]],
            "deployerFeeScale":"1"}}})");

}
