#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.registerAndAssociateNamedOutcomeFromTemplate("ab", 3, {"abc-outcome", {{"choice", "C"}}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "registerAndAssociateNamedOutcomeFromTemplate":{"question":3,
            "namedOutcomeTemplateInstance":{"id":"abc-outcome","keywordToValue":[["choice","C"]]}}}})");

}
