// HIP-3 perpDeploy actions. registerAsset and setOracle match the Python SDK
// (perp_deploy_*); the remaining variants follow the API docs.

#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");

    exchange.perpDeployRegisterAsset("abc", std::nullopt, "abc:BTC", 3, "65000", 1, false,
                                     hyperliquid::PerpDexSchema{"ABC Dex", 0, std::nullopt});
    const char* register_asset = R"({"type":"perpDeploy","registerAsset":{"maxGas":null,"assetRequest":{"coin":"abc:BTC","szDecimals":3,"oraclePx":"65000","marginTableId":1,"onlyIsolated":false},"dex":"abc","schema":{"fullName":"ABC Dex","collateralToken":0,"oracleUpdater":null}}})";
    assertSent(exchange, register_asset);
    assert(exchange.lastPayload().at("vaultAddress").is_null());
    assertPythonSignature(register_asset,
        "0x3d6ef12aa6c4f89ef7afc69e7d83e05c6f9854d00ab0dcf29382d3e12f0dc8da",
        "0x15593331c63f5980a63a6ccf578af7fd9430a47e184720d90f1b5f85a9faf56b", 27);

    exchange.perpDeployRegisterAsset("abc", 500, "abc:ETH", 2, "3000", 1, true);
    assertSent(exchange, R"({"type":"perpDeploy","registerAsset":{"maxGas":500,"assetRequest":{"coin":"abc:ETH","szDecimals":2,"oraclePx":"3000","marginTableId":1,"onlyIsolated":true},"dex":"abc","schema":null}})");

    exchange.perpDeployRegisterAsset2("abc", std::nullopt, "abc:SOL", 1, "150", 2, "noCross",
                                      hyperliquid::PerpDexSchema{"ABC Dex", 0, "0xABCDEF0123456789ABCDEF0123456789ABCDEF01"});
    assertSent(exchange, R"({"type":"perpDeploy","registerAsset2":{"maxGas":null,"assetRequest":{"coin":"abc:SOL","szDecimals":1,"oraclePx":"150","marginTableId":2,"marginMode":"noCross"},"dex":"abc","schema":{"fullName":"ABC Dex","collateralToken":0,"oracleUpdater":"0xabcdef0123456789abcdef0123456789abcdef01"}}})");

    assert(throws([&] { exchange.perpDeployRegisterAsset("", std::nullopt, "BTC", 3, "1", 1, false); }));

    // Inserted out of order; the maps sort them as the exchange requires.
    exchange.perpDeploySetOracle("abc", {{"abc:ETH", "3000"}, {"abc:BTC", "65000"}},
                                 {{{"abc:BTC", "65001"}}},
                                 {{"abc:ETH", "3001"}, {"abc:BTC", "65002"}});
    const char* set_oracle = R"({"type":"perpDeploy","setOracle":{"dex":"abc","oraclePxs":[["abc:BTC","65000"],["abc:ETH","3000"]],"markPxs":[[["abc:BTC","65001"]]],"externalPerpPxs":[["abc:BTC","65002"],["abc:ETH","3001"]]}})";
    assertSent(exchange, set_oracle);
    assertPythonSignature(set_oracle,
        "0x6a5da2494893701a659316043ff330f9f8b264b55277c05eb526af2c8f422c3",
        "0x65a1fd5c4ce20a54e7eebe52d33e8ca3cc01a336db3e6cf513eb9f69f230fbe1", 27);

    exchange.perpDeploySetFundingMultipliers({{"abc:ETH", "2"}, {"abc:BTC", "0.5"}});
    assertSent(exchange, R"({"type":"perpDeploy","setFundingMultipliers":[["abc:BTC","0.5"],["abc:ETH","2"]]})");

    exchange.perpDeployHaltTrading("abc:BTC", true);
    assertSent(exchange, R"({"type":"perpDeploy","haltTrading":{"coin":"abc:BTC","isHalted":true}})");

    exchange.perpDeploySetMarginTableIds({{"abc:BTC", 3}});
    assertSent(exchange, R"({"type":"perpDeploy","setMarginTableIds":[["abc:BTC",3]]})");

    exchange.perpDeploySetFeeRecipient("abc", "0xABCDEF0123456789ABCDEF0123456789ABCDEF01");
    assertSent(exchange, R"({"type":"perpDeploy","setFeeRecipient":{"dex":"abc","feeRecipient":"0xabcdef0123456789abcdef0123456789abcdef01"}})");

    exchange.perpDeploySetOpenInterestCaps({{"abc:BTC", 10000000}});
    assertSent(exchange, R"({"type":"perpDeploy","setOpenInterestCaps":[["abc:BTC",10000000]]})");

    exchange.perpDeploySetSubDeployers("abc", {{"setOracle", "0xABCDEF0123456789ABCDEF0123456789ABCDEF01", true}});
    assertSent(exchange, R"({"type":"perpDeploy","setSubDeployers":{"dex":"abc","subDeployers":[{"variant":"setOracle","user":"0xabcdef0123456789abcdef0123456789abcdef01","allowed":true}]}})");
    assert(throws([&] { exchange.perpDeploySetSubDeployers("abc", {}); }));

    exchange.perpDeploySetMarginModes({{"abc:BTC", "strictIsolated"}});
    assertSent(exchange, R"({"type":"perpDeploy","setMarginModes":[["abc:BTC","strictIsolated"]]})");

    exchange.perpDeploySetFeeScale("abc", "1.5");
    assertSent(exchange, R"({"type":"perpDeploy","setFeeScale":{"dex":"abc","scale":"1.5"}})");

    exchange.perpDeploySetGrowthModes({{"abc:BTC", true}});
    assertSent(exchange, R"({"type":"perpDeploy","setGrowthModes":[["abc:BTC",true]]})");
}
