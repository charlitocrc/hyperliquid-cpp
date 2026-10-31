#include "l1_action_signing_fixture.hpp"

namespace {

void cancelByCloidSignsItsOwnAction() {
    TestExchange exchange;

    exchange.cancelByCloid("BTC", hyperliquid::Cloid::fromInt(1));

    const auto& action = exchange.lastRequest().payload.at("action");
    assert(action.at("type") == "cancelByCloid");
    assert(action.at("cancels")[0].at("asset") == 0);
    assert(action.at("cancels")[0].at("cloid") == "0x00000000000000000000000000000001");
    assert(!action.at("cancels")[0].contains("a"));
    assert(!action.at("cancels")[0].contains("o"));

    nlohmann::ordered_json cancel_obj;
    cancel_obj["asset"] = 0;
    cancel_obj["cloid"] = "0x00000000000000000000000000000001";

    nlohmann::ordered_json expected_action;
    expected_action["type"] = "cancelByCloid";
    expected_action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    auto expected = hyperliquid::signL1Action(
        *wallet, expected_action, std::nullopt,
        exchange.lastRequest().payload.at("nonce").get<int64_t>(), std::nullopt,
        /*is_mainnet=*/false);
    assert(exchange.lastRequest().payload.at("signature") == expected.toJson());

    // Same action and nonce as the Python SDK reference above.
    auto pinned = hyperliquid::signL1Action(
        *wallet, expected_action, std::nullopt, 1583838, std::nullopt,
        /*is_mainnet=*/false).toJson();
    assert(pinned.at("r") == "0x923c4ea3515e7bc8243033f91c25dd1b8fb53f4c566179d66b8ab5f80f9fccf7");
    assert(pinned.at("s") == "0x2abfa94e0be12b0dbb0b49f0a1a3643ba74263505e1caa2daaf9a0a476fc381e");
    assert(pinned.at("v") == 27);
}

} // namespace

int main() {
    cancelByCloidSignsItsOwnAction();
    return 0;
}
