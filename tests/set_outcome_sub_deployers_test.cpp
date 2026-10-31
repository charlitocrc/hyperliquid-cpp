#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.setOutcomeSubDeployers(
        "ab", {{"settleOutcome", "0xABCDEF0123456789ABCDEF0123456789ABCDEF01", true},
               {"settleQuestion", "0x0000000000000000000000000000000000000002", false}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{"setSubDeployers":[
        {"variant":"settleOutcome","user":"0xabcdef0123456789abcdef0123456789abcdef01","allowed":true},
        {"variant":"settleQuestion","user":"0x0000000000000000000000000000000000000002","allowed":false}]}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.setOutcomeSubDeployers("ab", {}); }));
    assert(exchange.requests().size() == sent);
}
