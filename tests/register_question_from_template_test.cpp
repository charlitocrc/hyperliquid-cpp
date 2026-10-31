#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.registerQuestionFromTemplate(
        "ab", {"abc", {{"expiry", "20260801-1830"}}}, 0.25,
        {{"abc-outcome", {{"choice", "A"}}}, {"abc-outcome", {{"choice", "B"}}}, {"abc-other", {}}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "registerQuestionFromTemplate":{
            "questionTemplateInstance":{"id":"abc","keywordToValue":[["expiry","20260801-1830"]],
                                        "deployerFeeScale":"0.25"},
            "namedOutcomeTemplateInstances":[
                {"id":"abc-outcome","keywordToValue":[["choice","A"]]},
                {"id":"abc-outcome","keywordToValue":[["choice","B"]]},
                {"id":"abc-other","keywordToValue":[]}]}}})");

}
