#include "test_exchange.hpp"
#include <cassert>

using hyperliquid_test::TestExchange;

static void checkSignature(const TestExchange& exchange,
                           const nlohmann::ordered_json& action,
                           std::optional<std::string> vault) {
    const auto& payload = exchange.lastPayload();
    const auto wallet = hyperliquid::Wallet::fromPrivateKey(hyperliquid_test::TEST_PRIVATE_KEY);
    const auto signature = hyperliquid::signL1Action(*wallet, action, vault,
        payload.at("nonce").get<int64_t>(), 2000000000000LL, false);
    assert(payload.at("signature") == signature.toJson());
    assert(payload.at("action") == nlohmann::json(action));
    assert(payload.at("expiresAfter") == 2000000000000LL);
    assert(exchange.requests().back().path == "/exchange");
}

int main() {
    TestExchange exchange("0x1234567890abcdef1234567890abcdef12345678");
    exchange.setExpiresAfter(2000000000000LL);
    exchange.next_response = {{"status", "err"}, {"response", "rejected"}};
    assert(exchange.claimRewards() == exchange.next_response);
    checkSignature(exchange, {{"type", "claimRewards"}}, std::nullopt);
    assert(exchange.lastPayload().at("vaultAddress").is_null());
}
