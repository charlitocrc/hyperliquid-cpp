#pragma once

// Shared offline Exchange fixture for exchange-action tests. Captures every
// POST instead of hitting the network; metadata is injected so the
// constructor never fetches either.

#include "hyperliquid/exchange.hpp"

#include <string>
#include <vector>

namespace hyperliquid_test {

// Test-only key. Never used for anything but signing offline fixtures.
constexpr const char* TEST_PRIVATE_KEY =
    "0x0123456789012345678901234567890123456789012345678901234567890123";

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestExchange : public hyperliquid::Exchange {
public:
    explicit TestExchange(const std::string& vault_address = "")
        : Exchange(hyperliquid::Wallet::fromPrivateKey(TEST_PRIVATE_KEY),
                   "http://localhost",
                   &perpMeta(),
                   vault_address,
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

    const nlohmann::json& lastPayload() const {
        return requests_.back().payload;
    }

    // What post() returns; tests needing a specific response shape set this.
    nlohmann::json next_response = {{"status", "ok"}};

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return next_response;
    }

private:
    // BTC has szDecimals 5, ETH 4.
    static const hyperliquid::Meta& perpMeta() {
        static const hyperliquid::Meta meta{
            {hyperliquid::AssetInfo{"BTC", 5}, hyperliquid::AssetInfo{"ETH", 4}}};
        return meta;
    }

    static const hyperliquid::SpotMeta& spotMetaFixture() {
        static const hyperliquid::SpotMeta spot_meta{
            {hyperliquid::SpotAssetInfo{"PURR/USDC", {1, 0}, 0, true}},
            {
                hyperliquid::SpotTokenInfo{"USDC", 8, 8, 0, "0x0", true},
                hyperliquid::SpotTokenInfo{"PURR", 0, 5, 1, "0x1", true},
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

} // namespace hyperliquid_test
