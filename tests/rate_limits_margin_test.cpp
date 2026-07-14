#include "hyperliquid/exchange.hpp"

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
    static const Meta& perpMeta() {
        static const Meta meta{{AssetInfo{"BTC", 5}, AssetInfo{"ETH", 4}}};
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
    nlohmann::json next_response_ = {{"status", "ok"}};
};

// The action hash is keccak(msgpack(action)), so key order in the serialized
// action is load-bearing. Assert the literal serialization, not just json
// equality (which ignores order).
void topUpPostsExpectedAction() {
    TestExchange exchange;

    exchange.topUpIsolatedOnlyMargin("ETH", 5.0);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "topUpIsolatedOnlyMargin");
    assert(action.at("asset") == 1);  // ETH is index 1 in the fixture meta
    assert(action.at("leverage") == "5");  // float string, trailing zeros stripped
}

void topUpEncodesFractionalLeverageAsFloatString() {
    TestExchange exchange;

    exchange.topUpIsolatedOnlyMargin("BTC", 2.5);

    const auto& action = exchange.lastAction();
    assert(action.at("asset") == 0);
    assert(action.at("leverage") == "2.5");
}

void topUpRejectsNonPositiveLeverage() {
    TestExchange exchange;

    for (double bad : {0.0, -3.0}) {
        bool threw = false;
        try {
            exchange.topUpIsolatedOnlyMargin("ETH", bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }
    assert(exchange.requests().empty());  // nothing was sent
}

void reserveRequestWeightPostsExpectedAction() {
    TestExchange exchange;

    exchange.reserveRequestWeight(10);

    assert(exchange.requests().size() == 1);
    assert(exchange.requests()[0].path == "/exchange");

    const auto& action = exchange.lastAction();
    assert(action.at("type") == "reserveRequestWeight");
    assert(action.at("weight") == 10);

    // Signing of the envelope is covered by l1_action_signing_test.
}

void reserveRequestWeightRejectsNonPositiveWeight() {
    TestExchange exchange;

    for (int64_t bad : {int64_t{0}, int64_t{-1}}) {
        bool threw = false;
        try {
            exchange.reserveRequestWeight(bad);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        assert(threw);
    }
    assert(exchange.requests().empty());
}

} // namespace

int main() {
    topUpPostsExpectedAction();
    topUpEncodesFractionalLeverageAsFloatString();
    topUpRejectsNonPositiveLeverage();
    reserveRequestWeightPostsExpectedAction();
    reserveRequestWeightRejectsNonPositiveWeight();

    return 0;
}
