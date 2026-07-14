#include "hyperliquid/info.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Info;
using hyperliquid::Meta;
using hyperliquid::SpotAssetInfo;
using hyperliquid::SpotMeta;
using hyperliquid::SpotTokenInfo;

namespace {

struct CapturedRequest {
    std::string path;
    nlohmann::json payload;
};

class TestInfo : public Info {
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

protected:
    nlohmann::json post(const std::string& url_path,
                        const nlohmann::json& payload = nlohmann::json::object()) override {
        requests_.push_back({url_path, payload});
        return next_response_;
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
    nlohmann::json next_response_ = {
        {"ok", true},
        {"source", "mock"},
    };
};

void assertSingleRequest(const TestInfo& info, const nlohmann::json& expected_payload) {
    assert(info.requests().size() == 1);
    assert(info.requests()[0].path == "/info");
    assert(info.requests()[0].payload == expected_payload);
}

void metaAndAssetCtxsUsesDefaultDexWhenUnset() {
    TestInfo info;

    const auto response = info.metaAndAssetCtxs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "metaAndAssetCtxs"}});
}

void metaAndAssetCtxsIncludesDexWhenProvided() {
    TestInfo info;

    const auto response = info.metaAndAssetCtxs("test-dex");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "metaAndAssetCtxs"}, {"dex", "test-dex"}});
}

void spotMetaAndAssetCtxsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.spotMetaAndAssetCtxs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "spotMetaAndAssetCtxs"}});
}

void perpDexsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpDexs();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexs"}});
}

void queryPerpDeployAuctionStatusUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.queryPerpDeployAuctionStatus();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDeployAuctionStatus"}});
}

} // namespace

int main() {
    metaAndAssetCtxsUsesDefaultDexWhenUnset();
    metaAndAssetCtxsIncludesDexWhenProvided();
    spotMetaAndAssetCtxsUsesExpectedPayload();
    perpDexsUsesExpectedPayload();
    queryPerpDeployAuctionStatusUsesExpectedPayload();

    return 0;
}
