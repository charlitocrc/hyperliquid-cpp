#include "test_info.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using hyperliquid_test::assertSingleRequest;
using hyperliquid_test::TestInfo;

using hyperliquid::AssetInfo;
using hyperliquid::Meta;

namespace {

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
    metaParsesMarginTablesAndOptionalFields();
    metaWithoutMarginTablesParses();
    return 0;
}
