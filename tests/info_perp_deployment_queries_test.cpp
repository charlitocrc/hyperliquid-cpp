#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid::AssetInfo;
using hyperliquid::Meta;
using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

namespace {

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

void predictedFundingsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.predictedFundings();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "predictedFundings"}});
}

// Follows the meta()/metaAndAssetCtxs() convention: an empty dex is omitted
// rather than sent as "", which the API treats identically.
void perpsAtOpenInterestCapOmitsEmptyDex() {
    TestInfo info;

    const auto response = info.perpsAtOpenInterestCap();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpsAtOpenInterestCap"}});
}

void perpsAtOpenInterestCapIncludesDexWhenProvided() {
    TestInfo info;

    const auto response = info.perpsAtOpenInterestCap("xyz");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpsAtOpenInterestCap"}, {"dex", "xyz"}});
}

void perpCategoriesUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpCategories();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpCategories"}});
}

void perpConciseAnnotationsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpConciseAnnotations();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpConciseAnnotations"}});
}

void perpAnnotationUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpAnnotation("xyz:TSLA");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpAnnotation"}, {"coin", "xyz:TSLA"}});
}

// An empty coin comes back as null, which is exactly what an unannotated coin
// returns, so the mistake would otherwise look like a legitimate answer.
void perpAnnotationRejectsEmptyCoin() {
    TestInfo info;

    bool threw = false;
    try {
        info.perpAnnotation("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(info.requests().empty());
}

void perpDexLimitsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.perpDexLimits("xyz");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexLimits"}, {"dex", "xyz"}});
}

// "" is a valid dex elsewhere (it means the default/first dex), but this
// endpoint rejects it, so we fail before spending a request.
void perpDexLimitsRejectsEmptyDex() {
    TestInfo info;

    bool threw = false;
    try {
        info.perpDexLimits("");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(info.requests().empty());
}

// Here "" IS meaningful and must still be sent, not dropped from the payload.
void perpDexStatusSendsEmptyDexForFirstDex() {
    TestInfo info;

    const auto response = info.perpDexStatus();

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "perpDexStatus"}, {"dex", ""}});
}

void tokenDetailsUsesExpectedPayload() {
    TestInfo info;

    const auto response = info.tokenDetails("0xc1fb593aeffbeb02f85e0308e9956a90");

    assert(response == nlohmann::json({{"ok", true}, {"source", "mock"}}));
    assertSingleRequest(info, {{"type", "tokenDetails"},
                               {"tokenId", "0xc1fb593aeffbeb02f85e0308e9956a90"}});
}

// The meta response is the one place this SDK parses into a struct rather than
// handing back raw JSON, so unmodeled or misparsed fields are lost silently.
// Fixture shape is taken from a live builder-dex response: a fully-populated
// asset, a minimal one, and a marginTables entry whose id sits outside the
// table body as [id, table].
void metaParsesMarginTablesAndOptionalFields() {
    TestInfo info;
    info.setResponse({
        {"universe", {
            {
                {"name", "xyz:TSLA"},
                {"szDecimals", 3},
                {"maxLeverage", 20},
                {"marginTableId", 20},
                {"marginMode", "strictIsolated"},
                {"growthMode", "enabled"},
                {"lastGrowthModeChangeTime", "2025-11-23T17:37:10.033211662"},
                {"onlyIsolated", true},
                {"isDelisted", true},
            },
            // Everything past name/szDecimals/maxLeverage absent, as on the
            // default dex.
            {
                {"name", "BTC"},
                {"szDecimals", 5},
                {"maxLeverage", 40},
            },
        }},
        {"marginTables", {
            {50, {{"description", ""},
                  {"marginTiers", {{{"lowerBound", "0.0"}, {"maxLeverage", 50}}}}}},
            {51, {{"description", "tiered 10x"},
                  {"marginTiers", {{{"lowerBound", "0.0"}, {"maxLeverage", 10}},
                                   {{"lowerBound", "3000000.0"}, {"maxLeverage", 5}}}}}},
        }},
        {"collateralToken", 360},
    });

    const Meta meta = info.meta("xyz");

    assert(meta.universe.size() == 2);

    const auto& full = meta.universe[0];
    assert(full.name == "xyz:TSLA");
    assert(full.sz_decimals == 3);
    assert(full.max_leverage == 20);
    assert(full.margin_table_id.has_value() && full.margin_table_id.value() == 20);
    assert(full.margin_mode.has_value() && full.margin_mode.value() == "strictIsolated");
    assert(full.growth_mode.has_value() && full.growth_mode.value() == "enabled");
    assert(full.last_growth_mode_change_time.has_value());
    assert(full.only_isolated);
    assert(full.is_delisted);

    // Absent fields must default, not throw and not carry the previous asset's
    // values.
    const auto& minimal = meta.universe[1];
    assert(minimal.name == "BTC");
    assert(minimal.max_leverage == 40);
    assert(!minimal.margin_table_id.has_value());
    assert(!minimal.margin_mode.has_value());
    assert(!minimal.growth_mode.has_value());
    assert(!minimal.last_growth_mode_change_time.has_value());
    assert(!minimal.only_isolated);
    assert(!minimal.is_delisted);

    assert(meta.margin_tables.size() == 2);
    assert(meta.margin_tables[0].id == 50);
    assert(meta.margin_tables[0].description.empty());
    assert(meta.margin_tables[0].margin_tiers.size() == 1);

    const auto& tiered = meta.margin_tables[1];
    assert(tiered.id == 51);
    assert(tiered.description == "tiered 10x");
    assert(tiered.margin_tiers.size() == 2);
    assert(tiered.margin_tiers[0].lower_bound == "0.0");
    assert(tiered.margin_tiers[0].max_leverage == 10);
    assert(tiered.margin_tiers[1].lower_bound == "3000000.0");
    assert(tiered.margin_tiers[1].max_leverage == 5);

    assert(meta.collateral_token == 360);
}

// The default dex omits marginTables and collateralToken entirely.
void metaWithoutMarginTablesParses() {
    TestInfo info;
    info.setResponse({
        {"universe", {{{"name", "BTC"}, {"szDecimals", 5}}}},
    });

    const Meta meta = info.meta("");

    assert(meta.universe.size() == 1);
    assert(meta.universe[0].max_leverage == 0);
    assert(meta.margin_tables.empty());
    assert(meta.collateral_token == 0);
}

} // namespace

int main() {
    predictedFundingsUsesExpectedPayload();
    perpsAtOpenInterestCapOmitsEmptyDex();
    perpsAtOpenInterestCapIncludesDexWhenProvided();
    perpCategoriesUsesExpectedPayload();
    perpConciseAnnotationsUsesExpectedPayload();
    perpAnnotationUsesExpectedPayload();
    perpAnnotationRejectsEmptyCoin();
    perpDexLimitsUsesExpectedPayload();
    perpDexLimitsRejectsEmptyDex();
    perpDexStatusSendsEmptyDexForFirstDex();
    tokenDetailsUsesExpectedPayload();
    metaParsesMarginTablesAndOptionalFields();
    metaWithoutMarginTablesParses();
    metaAndAssetCtxsUsesDefaultDexWhenUnset();
    metaAndAssetCtxsIncludesDexWhenProvided();
    spotMetaAndAssetCtxsUsesExpectedPayload();
    perpDexsUsesExpectedPayload();
    queryPerpDeployAuctionStatusUsesExpectedPayload();

    return 0;
}
