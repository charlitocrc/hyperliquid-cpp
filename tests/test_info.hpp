#pragma once

// Shared offline Info fixture for info-query tests. Captures every POST
// instead of hitting the network; metadata is injected so the constructor
// never fetches either.

#include "hyperliquid/info.hpp"

#include <string>
#include <utility>
#include <vector>

namespace hyperliquid_test {

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestInfo : public hyperliquid::Info {
public:
    TestInfo()
        : Info("http://localhost",
               true,
               &perpMeta(),
               &spotMetaFixture(),
               &perpDexsFixture(),
               1000) {}

    const std::vector<CapturedRequest>& requests() const {
        return requests_;
    }

    void setResponse(nlohmann::json response) {
        next_response_ = std::move(response);
    }

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return next_response_;
    }

private:
    static const hyperliquid::Meta& perpMeta() {
        static const hyperliquid::Meta meta{{hyperliquid::AssetInfo{"BTC", 5}}};
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
    nlohmann::json next_response_ = {
        {"ok", true},
        {"source", "mock"},
    };
};

// Every info query is a thin POST wrapper, so the whole contract is: one
// request, to /info, with exactly this payload.
inline void assertSingleRequest(const TestInfo& info, const nlohmann::json& expected_payload) {
    assert(info.requests().size() == 1);
    assert(info.requests()[0].path == "/info");
    assert(info.requests()[0].payload == expected_payload);
}

} // namespace hyperliquid_test
