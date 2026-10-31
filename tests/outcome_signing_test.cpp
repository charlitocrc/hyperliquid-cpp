#include "outcomes_fixture.hpp"

int main() {
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
