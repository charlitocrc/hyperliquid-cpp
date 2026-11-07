// HIP-1/2 spotDeploy actions. Wire shapes and key order match the Python SDK
// (spot_deploy_*); variants it lacks (enableAlignedQuoteToken, blacklistUsers)
// follow the API docs.

#include "outcomes_fixture.hpp"

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");

    exchange.spotDeployRegisterToken("TEST", 2, 8, 1000000, "Test Token");
    const char* register_token = R"({"type":"spotDeploy","registerToken2":{"spec":{"name":"TEST","szDecimals":2,"weiDecimals":8},"maxGas":1000000,"fullName":"Test Token"}})";
    assertSent(exchange, register_token);
    // Deploys never carry the configured vault.
    assert(exchange.lastPayload().at("vaultAddress").is_null());
    assertPythonSignature(register_token,
        "0x82f54fd64f6b8dfc6762cbfe5b6b0b2df5d70300f5071461af161a98f900b62b",
        "0x12902f786d67435db213ae1957556e6046b381a556a8d0b4c878ed98b6f38708", 27);

    exchange.spotDeployUserGenesis(1234, {{"0xABCDEF0123456789ABCDEF0123456789ABCDEF01", "100000000"}},
                                   {{0, "5000"}});
    const char* user_genesis = R"({"type":"spotDeploy","userGenesis":{"token":1234,"userAndWei":[["0xabcdef0123456789abcdef0123456789abcdef01","100000000"]],"existingTokenAndWei":[[0,"5000"]]}})";
    assertSent(exchange, user_genesis);
    assertPythonSignature(user_genesis,
        "0xfc0b82ad6e1b2bfc8b88f00bf451ef5826c478fa3a538d250ce7daab6cf21016",
        "0x4ce6a2b87431fab1a585b79d1fd2e22489a37d4473387af40a76e06fb6f620fc", 28);

    exchange.spotDeployUserGenesis(1234, {}, {}, std::vector<std::pair<std::string, bool>>{
                                                     {"0x0000000000000000000000000000000000000002", true}});
    assertSent(exchange, R"({"type":"spotDeploy","userGenesis":{"token":1234,"userAndWei":[],"existingTokenAndWei":[],"blacklistUsers":[["0x0000000000000000000000000000000000000002",true]]}})");

    exchange.spotDeployEnableFreezePrivilege(1234);
    assertSent(exchange, R"({"type":"spotDeploy","enableFreezePrivilege":{"token":1234}})");
    exchange.spotDeployFreezeUser(1234, "0xABCDEF0123456789ABCDEF0123456789ABCDEF01", true);
    assertSent(exchange, R"({"type":"spotDeploy","freezeUser":{"token":1234,"user":"0xabcdef0123456789abcdef0123456789abcdef01","freeze":true}})");
    exchange.spotDeployRevokeFreezePrivilege(1234);
    assertSent(exchange, R"({"type":"spotDeploy","revokeFreezePrivilege":{"token":1234}})");
    exchange.spotDeployEnableQuoteToken(1234);
    assertSent(exchange, R"({"type":"spotDeploy","enableQuoteToken":{"token":1234}})");
    exchange.spotDeployEnableAlignedQuoteToken(1234);
    assertSent(exchange, R"({"type":"spotDeploy","enableAlignedQuoteToken":{"token":1234}})");

    exchange.spotDeployGenesis(1234, "100005000", false);
    assertSent(exchange, R"({"type":"spotDeploy","genesis":{"token":1234,"maxSupply":"100005000"}})");
    exchange.spotDeployGenesis(1234, "100005000", true);
    assertSent(exchange, R"({"type":"spotDeploy","genesis":{"token":1234,"maxSupply":"100005000","noHyperliquidity":true}})");

    exchange.spotDeployRegisterSpot(1234, 0);
    assertSent(exchange, R"({"type":"spotDeploy","registerSpot":{"tokens":[1234,0]}})");

    exchange.spotDeployRegisterHyperliquidity(56, 2.5, 100.0, 10);
    assertSent(exchange, R"({"type":"spotDeploy","registerHyperliquidity":{"spot":56,"startPx":"2.5","orderSz":"100","nOrders":10}})");
    exchange.spotDeployRegisterHyperliquidity(56, 2.5, 100.0, 10, 3);
    assertSent(exchange, R"({"type":"spotDeploy","registerHyperliquidity":{"spot":56,"startPx":"2.5","orderSz":"100","nOrders":10,"nSeededLevels":3}})");

    exchange.spotDeploySetDeployerTradingFeeShare(1234, "50%");
    assertSent(exchange, R"({"type":"spotDeploy","setDeployerTradingFeeShare":{"token":1234,"share":"50%"}})");
}
