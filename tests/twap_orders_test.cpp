#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Exchange;
using hyperliquid::Meta;
using hyperliquid::SpotAssetInfo;
using hyperliquid::SpotMeta;
using hyperliquid::SpotTokenInfo;
using hyperliquid::TwapWire;
using hyperliquid::Wallet;

namespace {

// Test-only key. Never used for anything but signing offline fixtures.
constexpr const char* TEST_PRIVATE_KEY =
    "0x0123456789012345678901234567890123456789012345678901234567890123";

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestExchange : public Exchange {
public:
    TestExchange()
        : Exchange(Wallet::fromPrivateKey(TEST_PRIVATE_KEY),
                   "http://localhost",
                   &perpMeta(),
                   "",
                   "",
                   &spotMetaFixture(),
                   &perpDexsFixture(),
                   1000) {}

    const std::vector<CapturedRequest>& requests() const {
        return requests_;
    }

    // The action as it was actually sent, minus the envelope.
    const nlohmann::json& lastAction() const {
        return requests_.back().payload.at("action");
    }

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return next_response_;
    }

private:
    // BTC has szDecimals 5, so sizes round to 5 decimal places.
    static const Meta& perpMeta() {
        static const Meta meta{{AssetInfo{"BTC", 5}}};
        return meta;
    }

    static const SpotMeta& spotMetaFixture() {
        static const SpotMeta spot_meta{
            {SpotAssetInfo{"PURR/USDC", {1, 0}, 0, true}},
            {
                SpotTokenInfo{"USDC", 8, 8, 0, "0x0", true},
                SpotTokenInfo{"PURR", 0, 5, 1, "0x1", true},
            },
        };
        return spot_meta;
    }

    static const std::vector<std::string>& perpDexsFixture() {
        static const std::vector<std::string> dexs{""};
        return dexs;
    }

    std::vector<CapturedRequest> requests_;
    nlohmann::json next_response_ = {
        {"status", "ok"},
        {"response", {{"type", "twapOrder"},
                      {"data", {{"status", {{"running", {{"twapId", 77738308}}}}}}}}},
    };
};

// The action hash is keccak(msgpack(action)), so the key order of the twap
// object is load-bearing: a wrong order still serializes to valid JSON but
// signs a different action and the API rejects the signature. Assert the
// literal serialization, not just json equality (which ignores order).
void twapWireSerializesKeysInSpecOrder() {
    TwapWire twap;
    twap.asset = 0;
    twap.is_buy = true;
    twap.size = "1.5";
    twap.reduce_only = false;
    twap.minutes = 30;
    twap.randomize = false;

    const std::string expected = R"({"a":0,"b":true,"s":"1.5","r":false,"m":30,"t":false})";
    assert(twap.toJson().dump() == expected);
}

void twapOrderPostsExpectedAction() {
    TestExchange exchange;

    const auto response = exchange.twapOrder("BTC", true, 1.5, 30);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "twapOrder");
    assert(action.at("twap").at("a") == 0);
    assert(action.at("twap").at("b") == true);
    assert(action.at("twap").at("s") == "1.5");
    assert(action.at("twap").at("r") == false);
    assert(action.at("twap").at("m") == 30);
    assert(action.at("twap").at("t") == false);

    // Envelope carries a nonce and a signature.
    const auto& payload = exchange.requests()[0].payload;
    assert(payload.contains("nonce"));
    assert(payload.at("nonce").get<int64_t>() > 0);
    assert(payload.at("signature").contains("r"));
    assert(payload.at("signature").contains("s"));
    assert(payload.at("signature").contains("v"));

    // The twap id comes back nested under status.running.
    assert(response.at("response").at("data").at("status").at("running").at("twapId") == 77738308);
}

void twapOrderRoundsSizeToSzDecimals() {
    TestExchange exchange;

    // BTC szDecimals is 5, so this must be truncated to 5 decimal places.
    exchange.twapOrder("BTC", true, 1.23456789, 30);

    assert(exchange.lastAction().at("twap").at("s") == "1.23457");
}

void twapOrderPropagatesFlags() {
    TestExchange exchange;

    exchange.twapOrder("BTC", false, 2.0, 60, /*reduce_only=*/true, /*randomize=*/true);

    const auto& twap = exchange.lastAction().at("twap");
    assert(twap.at("b") == false);
    assert(twap.at("r") == true);
    assert(twap.at("t") == true);
    assert(twap.at("m") == 60);
}

void twapOrderRejectsNonPositiveMinutes() {
    TestExchange exchange;

    bool threw = false;
    try {
        exchange.twapOrder("BTC", true, 1.0, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(exchange.requests().empty());  // nothing was sent
}

void twapOrderRejectsSizeThatRoundsToZero() {
    TestExchange exchange;

    // Below BTC's 5-decimal lot size: this would otherwise sign an order for 0.
    bool threw = false;
    try {
        exchange.twapOrder("BTC", true, 0.000001, 30);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
    assert(exchange.requests().empty());
}

void twapCancelPostsExpectedAction() {
    TestExchange exchange;

    exchange.twapCancel("BTC", 77738308);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "twapCancel");
    assert(action.at("a") == 0);
    assert(action.at("t") == 77738308);
}

void twapMethodsRejectUnknownCoin() {
    TestExchange exchange;

    bool threw = false;
    try {
        exchange.twapOrder("NOT_A_COIN", true, 1.0, 30);
    } catch (const std::exception&) {
        threw = true;
    }
    assert(threw);
}

} // namespace

int main() {
    twapWireSerializesKeysInSpecOrder();
    twapOrderPostsExpectedAction();
    twapOrderRoundsSizeToSzDecimals();
    twapOrderPropagatesFlags();
    twapOrderRejectsNonPositiveMinutes();
    twapOrderRejectsSizeThatRoundsToZero();
    twapCancelPostsExpectedAction();
    twapMethodsRejectUnknownCoin();

    return 0;
}
