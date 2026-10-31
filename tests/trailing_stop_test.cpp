#include "test_exchange.hpp"
#include <cassert>
#include <limits>
#include <stdexcept>

using hyperliquid_test::TestExchange;
using hyperliquid::RetracementUnit;

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
    const std::string vault = "0x1234567890abcdef1234567890abcdef12345678";
    TestExchange exchange(vault);
    exchange.setExpiresAfter(2000000000000LL);
    exchange.trailingStop("ETH", false, 1.23456, 1.234, RetracementUnit::Percent, true);
    checkSignature(exchange, {{"type", "trailingStop"}, {"asset", 1},
        {"isBuy", false}, {"sz", "1.2346"}, {"reduceOnly", true},
        {"retracement", {{"pct", "1.234%"}}}, {"activationPx", nullptr}}, vault);
    assert(exchange.lastPayload().at("vaultAddress") == vault);

    exchange.trailingStop("BTC", true, 0.1, 10, RetracementUnit::Price, false, 90000);
    checkSignature(exchange, {{"type", "trailingStop"}, {"asset", 0},
        {"isBuy", true}, {"sz", "0.1"}, {"reduceOnly", false},
        {"retracement", {{"px", "10"}}}, {"activationPx", "90000"}}, vault);

    TestExchange invalid;
    for (double bad : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
        for (int field = 0; field < 3; ++field) {
            bool threw = false;
            try {
                invalid.trailingStop("ETH", true, field == 0 ? bad : 1,
                    field == 1 ? bad : 1, RetracementUnit::Percent, false,
                    field == 2 ? bad : 100);
            } catch (const std::invalid_argument&) { threw = true; }
            assert(threw);
        }
    }
    bool threw = false;
    try { invalid.trailingStop("ETH", true, 0.000001, 1, RetracementUnit::Price); }
    catch (const std::invalid_argument&) { threw = true; }
    assert(threw);
    assert(invalid.requests().empty());
}
