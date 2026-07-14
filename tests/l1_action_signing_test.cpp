// Regression cover for the L1 action signing path shared by every exchange
// action (order, cancel, modify, leverage, scheduleCancel, twap, ...).
//
// Signing is RFC 6979 deterministic, so for a given (key, action, vault, nonce,
// expiresAfter, is_mainnet) the signature is reproducible. Each case below
// recomputes the expected signature independently and compares it to what the
// Exchange actually sent. That pins every input to signL1Action -- not just the
// action body, but the vault address, the expires-after, and the mainnet flag.
//
// This exists so the shared postL1Action() helper cannot silently drop or
// reorder a signing input.

#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/signing.hpp"

#include <cassert>
#include <string>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Exchange;
using hyperliquid::Meta;
using hyperliquid::Signature;
using hyperliquid::SpotAssetInfo;
using hyperliquid::SpotMeta;
using hyperliquid::SpotTokenInfo;
using hyperliquid::Wallet;

namespace {

constexpr const char* TEST_PRIVATE_KEY =
    "0x0123456789012345678901234567890123456789012345678901234567890123";

constexpr const char* TEST_VAULT = "0x1234567890abcdef1234567890abcdef12345678";

// Not MAINNET_API_URL, so is_mainnet must be false in the signature.
constexpr const char* TEST_URL = "http://localhost";

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestExchange : public Exchange {
public:
    explicit TestExchange(const std::string& vault_address = "")
        : Exchange(Wallet::fromPrivateKey(TEST_PRIVATE_KEY),
                   TEST_URL,
                   &perpMeta(),
                   vault_address,
                   "",
                   &spotMetaFixture(),
                   &perpDexsFixture(),
                   1000) {}

    const CapturedRequest& lastRequest() const { return requests_.back(); }
    size_t requestCount() const { return requests_.size(); }

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return nlohmann::json{{"status", "ok"}};
    }

private:
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
};

// Recompute the signature the Exchange should have produced, from the nonce it
// actually used, and assert the envelope matches in full.
void assertSignedCorrectly(const TestExchange& exchange,
                           const nlohmann::ordered_json& expected_action,
                           const std::string& vault_address,
                           std::optional<int64_t> expires_after) {
    assert(exchange.requestCount() == 1);

    const auto& request = exchange.lastRequest();
    assert(request.path == "/exchange");

    const auto& payload = request.payload;
    assert(payload.at("action") == nlohmann::json(expected_action));

    // The envelope reports the vault and expiry that were signed over.
    if (vault_address.empty()) {
        assert(payload.at("vaultAddress").is_null());
    } else {
        assert(payload.at("vaultAddress") == vault_address);
    }
    if (expires_after.has_value()) {
        assert(payload.at("expiresAfter") == expires_after.value());
    } else {
        assert(payload.at("expiresAfter").is_null());
    }

    const int64_t nonce = payload.at("nonce").get<int64_t>();
    assert(nonce > 0);

    auto wallet = Wallet::fromPrivateKey(TEST_PRIVATE_KEY);
    std::optional<std::string> vault_opt =
        vault_address.empty() ? std::nullopt : std::optional<std::string>(vault_address);

    const Signature expected = signL1Action(*wallet,
                                            expected_action,
                                            vault_opt,
                                            nonce,
                                            expires_after,
                                            /*is_mainnet=*/false);

    assert(payload.at("signature") == expected.toJson());
}

void cancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.cancel("BTC", 12345);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 12345;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void updateLeverageSignsExpectedAction() {
    TestExchange exchange;

    exchange.updateLeverage(10, "BTC", true);

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = 0;
    action["isCross"] = true;
    action["leverage"] = 10;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void scheduleCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.scheduleCancel(1700000000000);

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";
    action["time"] = 1700000000000;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// scheduleCancel() with no time omits the field entirely -- a different action
// body, so a different hash.
void scheduleCancelWithoutTimeOmitsField() {
    TestExchange exchange;

    exchange.scheduleCancel();

    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void twapOrderSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapOrder("BTC", true, 1.5, 30);

    nlohmann::ordered_json twap;
    twap["a"] = 0;
    twap["b"] = true;
    twap["s"] = "1.5";
    twap["r"] = false;
    twap["m"] = 30;
    twap["t"] = false;

    nlohmann::ordered_json action;
    action["type"] = "twapOrder";
    action["twap"] = twap;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

void twapCancelSignsExpectedAction() {
    TestExchange exchange;

    exchange.twapCancel("BTC", 77738308);

    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = 0;
    action["t"] = 77738308;

    assertSignedCorrectly(exchange, action, "", std::nullopt);
}

// The vault address is signed over, not just attached to the envelope. If the
// shared helper ever forgets to forward it, the recomputed signature diverges.
void vaultAddressIsSignedOver() {
    TestExchange exchange(TEST_VAULT);

    exchange.cancel("BTC", 999);

    nlohmann::ordered_json cancel_obj;
    cancel_obj["a"] = 0;
    cancel_obj["o"] = 999;

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = nlohmann::ordered_json::array({cancel_obj});

    assertSignedCorrectly(exchange, action, TEST_VAULT, std::nullopt);
}

// Likewise expiresAfter: it feeds the action hash.
void expiresAfterIsSignedOver() {
    TestExchange exchange;
    exchange.setExpiresAfter(1800000000000);

    exchange.updateLeverage(5, "BTC", false);

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = 0;
    action["isCross"] = false;
    action["leverage"] = 5;

    assertSignedCorrectly(exchange, action, "", 1800000000000);
}

// Both at once, which is the combination most likely to be dropped by a
// refactor that only threads one of them through.
void vaultAndExpiresAfterTogether() {
    TestExchange exchange(TEST_VAULT);
    exchange.setExpiresAfter(1800000000000);

    exchange.twapCancel("BTC", 42);

    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = 0;
    action["t"] = 42;

    assertSignedCorrectly(exchange, action, TEST_VAULT, 1800000000000);
}

} // namespace

int main() {
    cancelSignsExpectedAction();
    updateLeverageSignsExpectedAction();
    scheduleCancelSignsExpectedAction();
    scheduleCancelWithoutTimeOmitsField();
    twapOrderSignsExpectedAction();
    twapCancelSignsExpectedAction();
    vaultAddressIsSignedOver();
    expiresAfterIsSignedOver();
    vaultAndExpiresAfterTogether();

    return 0;
}
