// HIP-4 outcomes: coin/asset encoding, order rounding, and the userOutcome /
// outcome deployer actions.
//
// The Python SDK has no outcome support, so the wire shapes are pinned against
// the API docs. Each expected action below is the docs' JSON parsed into an
// ordered_json, and the request's signature is recomputed from it: L1 actions
// are signed over their msgpack bytes, so this fails if the SDK sends the right
// keys in a different order than the docs, not just if a field is wrong.

#include "test_exchange.hpp"

#include "hyperliquid/utils/conversions.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using hyperliquid_test::TEST_PRIVATE_KEY;
using hyperliquid_test::TestExchange;

namespace {

template <typename F>
bool throws(F&& f) {
    try {
        f();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void assertSent(const TestExchange& exchange, const char* expected_json,
                bool with_vault = false, const std::string& vault = "") {
    const auto expected = nlohmann::ordered_json::parse(expected_json);
    const auto& payload = exchange.lastPayload();
    assert(payload.at("action") == nlohmann::json::parse(expected_json));

    const auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    const auto signature = hyperliquid::signL1Action(
        *wallet, expected,
        with_vault ? std::optional<std::string>(vault) : std::nullopt,
        payload.at("nonce").get<int64_t>(), std::nullopt, /*is_mainnet=*/false);
    assert(payload.at("signature") == signature.toJson());
}

// assertSent() checks the SDK against signL1Action(); this checks
// signL1Action() itself against the Python SDK on the outcome shapes (null
// amount, nested tuple arrays), with
//   sign_l1_action(wallet, action, None, 1583838, None, False)
void signaturesMatchPythonSdk() {
    const auto wallet = hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    auto sign = [&](const char* action_json) {
        return hyperliquid::signL1Action(*wallet, nlohmann::ordered_json::parse(action_json),
                                         std::nullopt, 1583838, std::nullopt, false)
            .toJson();
    };

    auto merge = sign(R"({"type":"userOutcome","mergeOutcome":{"outcome":7,"amount":null}})");
    assert(merge.at("r") == "0x2142f52a87e12d1ec005a0a5292c5081f5f657a05671223a5574c3ee28484366");
    assert(merge.at("s") == "0x57b94d83148e709de4b18fd71a1892c6d528b095c1940c53c54b7afaf9b55aef");
    assert(merge.at("v") == 28);

    auto settle = sign(R"({"type":"outcomeDeploy","venue":"ab","operation":{"settleQuestion2":{
        "question":3,"outcomeSettlements":[{"outcome":11,"settleFraction":"1","details":"",
        "nameAndDescription":["template:abc-outcome","choice:A"],"sideNames":["Yes","No"]}],
        "nameAndDescription":["template:abc","expiry:20260801-1830"]}}})");
    assert(settle.at("r") == "0x17fa99dbd011ba5ecfd3e4f0c463841d448c749264521fe2bad1dc466ffbc379");
    assert(settle.at("s") == "0x37bd4b26f72a49bfde8c74dcafb91ad6a889d7b1e346557d50fae59a0a35eb95");
    assert(settle.at("v") == 27);
}

void coinEncoding() {
    // Docs example: outcome 1, side 0 -> #10 / asset 100000010.
    assert(hyperliquid::outcomeCoin(1, 0) == "#10");
    assert(hyperliquid::outcomeAsset(1, 0) == 100000010);
    assert(hyperliquid::outcomeCoin(14200, 1) == "#142001");
    assert(throws([] { hyperliquid::outcomeCoin(1, 2); }));
    assert(throws([] { hyperliquid::outcomeCoin(-1, 0); }));
    assert(throws([] { hyperliquid::outcomeAsset(300000000, 0); }));  // past int

    TestExchange exchange;
    const auto& info = exchange.info_;
    assert(info.nameToAsset("#10") == 100000010);
    assert(info.nameToAsset("#0") == 100000000);
    assert(info.nameToCoin("#142001") == "#142001");
    for (const char* bad : {"#", "#12", "#1a0", "#-10", "#99999999990"}) {
        assert(throws([&] { info.nameToAsset(bad); }));
        assert(throws([&] { info.nameToCoin(bad); }));
    }
}

void outcomeOrderRoundsToWholeSharesAndFiveDecimals() {
    TestExchange exchange;
    hyperliquid::OrderType limit;
    limit.limit = hyperliquid::LimitOrderType{"Gtc"};

    exchange.order("#142000", true, 10.6, 0.2859349, limit);
    auto wire = exchange.lastAction().at("orders").at(0);
    assert(wire.at("a") == 100142000);
    assert(wire.at("p") == "0.28593");
    assert(wire.at("s") == "11");

    // The generic spot rule would allow 0.012346 here.
    exchange.order("#142001", false, 3, 0.0123456, limit);
    assert(exchange.lastAction().at("orders").at(0).at("p") == "0.01235");
}

void outcomeMarketOrderStaysInsideZeroOne() {
    TestExchange exchange;
    exchange.marketOpen("#142000", true, 5, 0.98);  // 0.98 * 1.05 = 1.029
    assert(exchange.lastAction().at("orders").at(0).at("p") == "0.99999");
}

// HIP-3 perps sit above the spot id range but take perp (6-decimal) ticks.
void builderPerpUsesPerpTick() {
    TestExchange exchange;
    exchange.info_.registerPerpMeta(hyperliquid::Meta{{hyperliquid::AssetInfo{"test:ABC", 0}}}, 110000);
    hyperliquid::OrderType limit;
    limit.limit = hyperliquid::LimitOrderType{"Gtc"};

    exchange.order("test:ABC", true, 1, 0.0012345678, limit);
    auto wire = exchange.lastAction().at("orders").at(0);
    assert(wire.at("a") == 110000);
    assert(wire.at("p") == "0.001235");  // spot's 8 decimals would send 0.0012346
}

void userOutcomeActions() {
    TestExchange exchange;

    exchange.splitOutcome(7, 123);
    assertSent(exchange, R"({"type":"userOutcome","splitOutcome":{"outcome":7,"amount":"123"}})");

    exchange.mergeOutcome(7, 2.5);
    assertSent(exchange, R"({"type":"userOutcome","mergeOutcome":{"outcome":7,"amount":"2.5"}})");

    exchange.mergeOutcome(7);
    assertSent(exchange, R"({"type":"userOutcome","mergeOutcome":{"outcome":7,"amount":null}})");

    exchange.mergeQuestion(3);
    assertSent(exchange, R"({"type":"userOutcome","mergeQuestion":{"question":3,"amount":null}})");

    exchange.negateOutcome(3, 11, 4);
    assertSent(exchange,
               R"({"type":"userOutcome","negateOutcome":{"question":3,"outcome":11,"amount":"4"}})");

    // A configured subaccount trades its own outcome balances, as with orders.
    const std::string vault = "0x1234567890abcdef1234567890abcdef12345678";
    TestExchange sub(vault);
    sub.splitOutcome(7, 1);
    assert(sub.lastPayload().at("vaultAddress") == vault);
    assertSent(sub, R"({"type":"userOutcome","splitOutcome":{"outcome":7,"amount":"1"}})", true, vault);

    assert(throws([&] { exchange.splitOutcome(7, 0); }));
    assert(throws([&] { exchange.mergeOutcome(7, -1.0); }));
    assert(throws([&] { exchange.negateOutcome(3, 11, 0); }));
    assert(exchange.requests().size() == 5);
}

void deployerActions() {
    // Deployer actions ignore a configured vault, in the signature and envelope.
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");

    exchange.activateOutcomeDeployer("ab");
    assertSent(exchange, R"({"type":"activateOutcomeDeployer","activate":{"venueName":"ab"}})");
    assert(exchange.lastPayload().at("vaultAddress").is_null());

    exchange.deactivateOutcomeDeployer();
    assertSent(exchange, R"({"type":"activateOutcomeDeployer","deactivate":null})");

    // Keywords given out of order go out sorted.
    hyperliquid::OutcomeTemplateInstance standalone{
        "abc", {{"underlying", "ABC"}, {"expiry", "20260801-0600"}, {"target", "100"}}};
    exchange.registerStandaloneOutcomeFromTemplate("ab", standalone, 1);
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "registerStandaloneOutcomeFromTemplate":{"id":"abc",
            "keywordToValue":[["expiry","20260801-0600"],["target","100"],["underlying","ABC"]],
            "deployerFeeScale":"1"}}})");

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

    exchange.registerAndAssociateNamedOutcomeFromTemplate("ab", 3, {"abc-outcome", {{"choice", "C"}}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "registerAndAssociateNamedOutcomeFromTemplate":{"question":3,
            "namedOutcomeTemplateInstance":{"id":"abc-outcome","keywordToValue":[["choice","C"]]}}}})");

    exchange.settleOutcome("ab", {7, 1,
                                  {"template:abc", "expiry:20260801-0600|target:100|underlying:ABC"},
                                  {"template:Over", "template:Under"}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{
        "settleOutcome":{"outcome":7,"settleFraction":"1","details":"",
            "nameAndDescription":["template:abc","expiry:20260801-0600|target:100|underlying:ABC"],
            "sideNames":["template:Over","template:Under"]}}})");

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

    exchange.setOutcomeSubDeployers(
        "ab", {{"settleOutcome", "0xABCDEF0123456789ABCDEF0123456789ABCDEF01", true},
               {"settleQuestion", "0x0000000000000000000000000000000000000002", false}});
    assertSent(exchange, R"({"type":"outcomeDeploy","venue":"ab","operation":{"setSubDeployers":[
        {"variant":"settleOutcome","user":"0xabcdef0123456789abcdef0123456789abcdef01","allowed":true},
        {"variant":"settleQuestion","user":"0x0000000000000000000000000000000000000002","allowed":false}]}})");

    const auto sent = exchange.requests().size();
    assert(throws([&] { exchange.activateOutcomeDeployer(""); }));
    assert(throws([&] { exchange.settleOutcome("", {}); }));
    assert(throws([&] { exchange.settleQuestion("ab", 3, {}, {"q", "d"}); }));
    assert(throws([&] { exchange.setOutcomeSubDeployers("ab", {}); }));
    assert(exchange.requests().size() == sent);
}

}  // namespace

int main() {
    signaturesMatchPythonSdk();
    coinEncoding();
    outcomeOrderRoundsToWholeSharesAndFiveDecimals();
    outcomeMarketOrderStaysInsideZeroOne();
    builderPerpUsesPerpTick();
    userOutcomeActions();
    deployerActions();
    return 0;
}
