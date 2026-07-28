// Pins the typed views over websocket payloads.
//
// The payloads below are the real wire shapes, not the ones the docs describe.
// That distinction is the whole point of this file: the TypeScript definitions
// declare markPx, accountValue, candle OHLCV and friends as `number`, and the
// live API sends every one of them as a decimal string. A strict double
// conversion compiles fine and throws on the first real message, so these tests
// feed string-valued numbers deliberately.

#include "hyperliquid/ws_types.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

using namespace hyperliquid;
using nlohmann::json;

namespace {

int checks_run = 0;

bool near(double actual, double expected) {
    return std::fabs(actual - expected) < 1e-9;
}

// Shapes confirmed against api.hyperliquid.xyz.
void testMarketDataFromRealShapes() {
    const json book_payload{
        {"coin", "ETH"},
        {"time", 1785245160000},
        {"levels", json::array({
            json::array({{{"px", "1894.9"}, {"sz", "12.5"}, {"n", 3}}}),
            json::array({{{"px", "1895.0"}, {"sz", "8.0"}, {"n", 2}}}),
        })},
    };
    const auto book = book_payload.get<WsBook>();
    assert(book.coin == "ETH");
    assert(book.time == 1785245160000LL);
    assert(book.levels[0].size() == 1 && book.levels[1].size() == 1);
    assert(book.levels[0][0].px == "1894.9");  // bids
    assert(book.levels[0][0].n == 3);
    assert(book.levels[1][0].px == "1895.0");  // asks
    ++checks_run;

    // Candle o/c/h/l/v are documented as numbers and sent as strings.
    const json candle_payload{
        {"t", 1785245160000}, {"T", 1785245219999}, {"s", "ETH"}, {"i", "1m"},
        {"o", "1886.3"}, {"c", "1885.5"}, {"h", "1886.3"}, {"l", "1885.2"},
        {"v", "66.4384"}, {"n", 93},
    };
    const auto candle = candle_payload.get<Candle>();
    assert(candle.coin == "ETH" && candle.interval == "1m");
    assert(near(candle.open, 1886.3) && near(candle.close, 1885.5));
    assert(near(candle.high, 1886.3) && near(candle.low, 1885.2));
    assert(near(candle.volume, 66.4384));
    assert(candle.num_trades == 93);
    assert(candle.open_millis == 1785245160000LL);
    ++checks_run;

    const json trade_payload{
        {"coin", "ETH"}, {"side", "A"}, {"px", "1894.8"}, {"sz", "0.0064"},
        {"hash", "0xabc"}, {"time", 1785245160000}, {"tid", 123456789},
        {"users", json::array({"0xbuyer", "0xseller"})},
    };
    const auto trade = trade_payload.get<WsTrade>();
    assert(trade.px == "1894.8" && trade.tid == 123456789LL);
    assert(trade.users[0] == "0xbuyer" && trade.users[1] == "0xseller");
    ++checks_run;
}

// Either side of the bbo is null when that side of the book is empty, which the
// generic array conversion cannot express.
void testBboHandlesEmptySide() {
    const json both{
        {"coin", "ETH"}, {"time", 1},
        {"bbo", json::array({{{"px", "1"}, {"sz", "2"}, {"n", 1}},
                             {{"px", "3"}, {"sz", "4"}, {"n", 2}}})},
    };
    const auto full = both.get<WsBbo>();
    assert(full.bbo[0].has_value() && full.bbo[1].has_value());
    assert(full.bbo[0]->px == "1" && full.bbo[1]->sz == "4");
    ++checks_run;

    const json one_sided{
        {"coin", "ETH"}, {"time", 1},
        {"bbo", json::array({nullptr, {{"px", "3"}, {"sz", "4"}, {"n", 2}}})},
    };
    const auto partial = one_sided.get<WsBbo>();
    assert(!partial.bbo[0].has_value());
    assert(partial.bbo[1].has_value() && partial.bbo[1]->px == "3");
    ++checks_run;
}

// Asset contexts are the worst offenders: every field is documented as a number
// and every field arrives as a string.
void testAssetCtxAcceptsStringNumbers() {
    const json perp_payload{
        {"funding", "0.0000125"}, {"openInterest", "36758.9321"},
        {"prevDayPx", "65637.0"}, {"dayNtlVlm", "2090688471.377"},
        {"oraclePx", "63340.0"}, {"markPx", "63319.0"}, {"midPx", "63310.5"},
    };
    const auto ctx = perp_payload.get<PerpsAssetCtx>();
    assert(near(ctx.mark_px, 63319.0));
    assert(near(ctx.funding, 0.0000125));
    assert(near(ctx.open_interest, 36758.9321));
    assert(ctx.mid_px.has_value() && near(*ctx.mid_px, 63310.5));
    ++checks_run;

    // A JSON number must work too, in case the wire ever matches the docs.
    json numeric_payload = perp_payload;
    numeric_payload["markPx"] = 63319.0;
    assert(near(numeric_payload.get<PerpsAssetCtx>().mark_px, 63319.0));
    ++checks_run;

    // midPx is optional and can be absent on a one-sided book.
    json no_mid = perp_payload;
    no_mid.erase("midPx");
    assert(!no_mid.get<PerpsAssetCtx>().mid_px.has_value());
    ++checks_run;
}

void testClearinghouseStateAcceptsStringNumbers() {
    const json margin{
        {"accountValue", "1234.5"}, {"totalNtlPos", "0.0"},
        {"totalRawUsd", "1234.5"}, {"totalMarginUsed", "0.0"},
    };
    const json payload{
        {"dex", ""}, {"user", "0xabc"},
        {"clearinghouseState", {
            {"assetPositions", json::array({{{"type", "oneWay"},
                                             {"position", {{"coin", "ETH"}, {"szi", "1.0"}}}}})},
            {"marginSummary", margin},
            {"crossMarginSummary", margin},
            {"crossMaintenanceMarginUsed", "0.0"},
            {"withdrawable", "1234.5"},
        }},
    };
    const auto state = payload.get<ClearinghouseState>();
    assert(near(state.clearinghouse_state.margin_summary.account_value, 1234.5));
    assert(near(state.clearinghouse_state.withdrawable, 1234.5));
    assert(state.clearinghouse_state.asset_positions.size() == 1);
    assert(state.clearinghouse_state.asset_positions[0].type == "oneWay");
    // Position stays raw JSON: the websocket docs never define its fields.
    assert(state.clearinghouse_state.asset_positions[0].position["coin"] == "ETH");
    ++checks_run;
}

// The union is expressed by key presence, so exactly one member is populated.
void testUserEventPopulatesOneMember() {
    const json fill{
        {"coin", "ETH"}, {"px", "1894.8"}, {"sz", "0.5"}, {"side", "B"},
        {"time", 1}, {"startPosition", "0.0"}, {"dir", "Open Long"},
        {"closedPnl", "0.0"}, {"hash", "0xabc"}, {"oid", 42}, {"crossed", true},
        {"fee", "0.01"}, {"tid", 7}, {"feeToken", "USDC"},
    };
    const auto fills_event = json{{"fills", json::array({fill})}}.get<WsUserEvent>();
    assert(fills_event.fills.has_value() && fills_event.fills->size() == 1);
    assert(fills_event.fills->front().oid == 42);
    assert(!fills_event.fills->front().liquidation.has_value());
    assert(!fills_event.fills->front().builder_fee.has_value());
    assert(!fills_event.funding.has_value());
    assert(!fills_event.liquidation.has_value());
    assert(!fills_event.non_user_cancel.has_value());
    ++checks_run;

    const auto cancel_event =
        json{{"nonUserCancel", json::array({{{"coin", "ETH"}, {"oid", 9}}})}}.get<WsUserEvent>();
    assert(!cancel_event.fills.has_value());
    assert(cancel_event.non_user_cancel.has_value());
    assert(cancel_event.non_user_cancel->front().oid == 9);
    ++checks_run;

    // Optional sub-objects populate when present.
    json liquidated_fill = fill;
    liquidated_fill["liquidation"] = {{"markPx", "1900.0"}, {"method", "market"}};
    liquidated_fill["builderFee"] = "0.001";
    const auto parsed = liquidated_fill.get<WsFill>();
    assert(parsed.liquidation.has_value());
    assert(near(parsed.liquidation->mark_px, 1900.0));
    assert(parsed.liquidation->method == "market");
    assert(!parsed.liquidation->liquidated_user.has_value());
    assert(parsed.builder_fee.has_value() && *parsed.builder_fee == "0.001");
    ++checks_run;
}

void testOrderUpdate() {
    const json payload{
        {"order", {{"coin", "ETH"}, {"side", "B"}, {"limitPx", "1800.0"},
                   {"sz", "1.0"}, {"oid", 55}, {"timestamp", 123}, {"origSz", "2.0"}}},
        {"status", "open"},
        {"statusTimestamp", 456},
    };
    const auto order = payload.get<WsOrder>();
    assert(order.order.oid == 55 && order.order.orig_sz == "2.0");
    assert(!order.order.cloid.has_value());
    assert(order.status == "open" && order.status_timestamp == 456);
    ++checks_run;
}

// Deltas are a genuinely tagged union, dispatched on "type".
void testLedgerUpdateVariantDispatch() {
    const auto deposit = json{
        {"time", 1}, {"hash", "0xabc"},
        {"delta", {{"type", "deposit"}, {"usdc", "500.0"}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    assert(std::holds_alternative<WsDeposit>(deposit.delta));
    assert(near(std::get<WsDeposit>(deposit.delta).usdc, 500.0));
    ++checks_run;

    const auto withdraw = json{
        {"time", 2}, {"hash", "0xdef"},
        {"delta", {{"type", "withdraw"}, {"usdc", "100.0"}, {"nonce", 7}, {"fee", "1.0"}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    assert(std::holds_alternative<WsWithdraw>(withdraw.delta));
    assert(std::get<WsWithdraw>(withdraw.delta).nonce == 7);
    ++checks_run;

    // The three vault deltas share a layout and differ only by the tag, so they
    // share one struct that keeps the tag.
    for (const std::string tag : {"vaultCreate", "vaultDeposit", "vaultDistribution"}) {
        const auto vault = json{
            {"time", 3}, {"hash", "0x1"},
            {"delta", {{"type", tag}, {"vault", "0xvault"}, {"usdc", "10.0"}}},
        }.get<WsUserNonFundingLedgerUpdate>();
        assert(std::holds_alternative<WsVaultDelta>(vault.delta));
        assert(std::get<WsVaultDelta>(vault.delta).type == tag);
    }
    ++checks_run;

    const auto liquidation = json{
        {"time", 4}, {"hash", "0x2"},
        {"delta", {{"type", "liquidation"}, {"accountValue", "50.0"},
                   {"leverageType", "Cross"},
                   {"liquidatedPositions", json::array({{{"coin", "ETH"}, {"szi", "-1.5"}}})}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    const auto& liq = std::get<WsLedgerLiquidation>(liquidation.delta);
    assert(liq.leverage_type == "Cross");
    assert(liq.liquidated_positions.size() == 1);
    assert(near(liq.liquidated_positions[0].szi, -1.5));
    ++checks_run;

    const auto class_transfer = json{
        {"time", 5}, {"hash", "0x3"},
        {"delta", {{"type", "accountClassTransfer"}, {"usdc", "25.0"}, {"toPerp", true}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    assert(std::get<WsAccountClassTransfer>(class_transfer.delta).to_perp);
    ++checks_run;

    // `send` is absent from the docs' list of delta types but is what real
    // accounts receive. Distinct from spotTransfer: it carries dex routing.
    const auto send = json{
        {"time", 6}, {"hash", "0x4"},
        {"delta", {{"type", "send"}, {"token", "USDC"}, {"amount", "1916.73"},
                   {"usdcValue", "1916.73"}, {"user", "0xabc"}, {"destination", "0xdef"},
                   {"fee", "1.0"}, {"feeToken", "USDC"}, {"nativeTokenFee", "0.0"},
                   {"nonce", 1772012354684}, {"sourceDex", ""}, {"destinationDex", ""}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    const auto& sent = std::get<WsSend>(send.delta);
    assert(near(sent.amount, 1916.73) && sent.fee_token == "USDC");
    assert(sent.nonce == 1772012354684LL);
    ++checks_run;

    // An unmodelled type must survive as WsUnknownDelta rather than throwing:
    // `send` proved the documented list is incomplete, and one unrecognised
    // entry must not make a whole account's feed unreadable.
    const auto unknown = json{
        {"time", 7}, {"hash", "0x5"},
        {"delta", {{"type", "somethingNewIn2027"}, {"usdc", "1.0"}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    assert(std::holds_alternative<WsUnknownDelta>(unknown.delta));
    const auto& raw = std::get<WsUnknownDelta>(unknown.delta);
    assert(raw.type == "somethingNewIn2027");
    assert(raw.raw["usdc"] == "1.0");  // payload preserved, not discarded
    // Critically, it must not masquerade as a decoded variant.
    assert(!std::holds_alternative<WsDeposit>(unknown.delta));
    ++checks_run;
}

void testTwapTypes() {
    const json state{
        {"coin", "ETH"}, {"user", "0xabc"}, {"side", "B"}, {"sz", "10.0"},
        {"executedSz", "2.5"}, {"executedNtl", "4700.0"}, {"minutes", 30},
        {"reduceOnly", false}, {"randomize", true}, {"timestamp", 111},
    };
    const auto history = json{
        {"state", state},
        {"status", {{"status", "activated"}, {"description", "running"}}},
        {"time", 222},
    }.get<WsTwapHistory>();
    assert(history.state.minutes == 30 && history.state.randomize);
    assert(near(history.state.executed_sz, 2.5));
    assert(history.status == "activated" && history.status_description == "running");
    ++checks_run;

    // Regression: the docs mark `description` required, but every live status
    // object carries only {"status": ...}. Requiring it threw on real payloads.
    const auto no_description = json{
        {"state", state},
        {"status", {{"status", "activated"}}},
        {"time", 1782481547},
        {"twapId", 1975664},
    }.get<WsTwapHistory>();
    assert(no_description.status == "activated");
    assert(!no_description.status_description.has_value());
    assert(no_description.twap_id.has_value() && *no_description.twap_id == 1975664);
    ++checks_run;

    // Undocumented but present on the wire: nullable stopPx and trigger on the
    // state. Unknown fields must be ignored, not rejected.
    json state_with_extras = state;
    state_with_extras["stopPx"] = nullptr;
    state_with_extras["trigger"] = nullptr;
    assert(state_with_extras.get<TwapState>().coin == "ETH");
    ++checks_run;

    // twapStates arrives as (twap id, state) pairs.
    const auto states = json{
        {"dex", ""}, {"user", "0xabc"},
        {"states", json::array({json::array({77, state})})},
    }.get<TwapStates>();
    assert(states.states.size() == 1);
    assert(states.states[0].first == 77);
    assert(states.states[0].second.coin == "ETH");
    ++checks_run;
}

void testSpotAndAllDexsState() {
    const auto spot = json{
        {"user", "0xabc"},
        {"spotState", {{"balances", json::array({{{"coin", "HYPE"}, {"token", 150},
                                                  {"hold", "0.0"}, {"total", "5.5"},
                                                  {"entryNtl", "100.0"}}})}}},
    }.get<WsSpotState>();
    assert(spot.spot_state.balances.size() == 1);
    assert(spot.spot_state.balances[0].coin == "HYPE");
    assert(spot.spot_state.balances[0].token == 150);
    assert(spot.spot_state.balances[0].total == "5.5");
    ++checks_run;

    const json margin{{"accountValue", "1.0"}, {"totalNtlPos", "0.0"},
                      {"totalRawUsd", "1.0"}, {"totalMarginUsed", "0.0"}};
    const json inner{{"assetPositions", json::array()}, {"marginSummary", margin},
                     {"crossMarginSummary", margin}, {"crossMaintenanceMarginUsed", "0.0"},
                     {"withdrawable", "1.0"}};
    const auto all_dexs = json{
        {"user", "0xabc"},
        {"clearinghouseStates", json::array({json::array({"testdex", inner})})},
    }.get<WsAllDexsClearinghouseState>();
    assert(all_dexs.clearinghouse_states.size() == 1);
    assert(all_dexs.clearinghouse_states[0].first == "testdex");
    assert(near(all_dexs.clearinghouse_states[0].second.margin_summary.account_value, 1.0));
    ++checks_run;
}

// The two envelopes the docs never define. Field names were read off live
// payloads, so these tests are what pins them.
void testUndocumentedEnvelopes() {
    const auto fundings = json{
        {"isSnapshot", true},
        {"user", "0xabc"},
        {"fundings", json::array({{{"coin", "BTC"}, {"fundingRate", "0.0000125"},
                                   {"nSamples", nullptr}, {"szi", "-7.65321"},
                                   {"time", 1784890800034}, {"usdc", "6.213449"}}})},
    }.get<WsUserFundings>();
    assert(fundings.is_snapshot.has_value() && *fundings.is_snapshot);
    assert(fundings.fundings.size() == 1);
    assert(fundings.fundings[0].coin == "BTC");
    assert(fundings.fundings[0].usdc == "6.213449");  // nSamples ignored
    ++checks_run;

    const auto ledger = json{
        {"isSnapshot", true},
        {"user", "0xabc"},
        {"nonFundingLedgerUpdates", json::array({
            {{"time", 1}, {"hash", "0xabc"},
             {"delta", {{"type", "deposit"}, {"usdc", "500.0"}}}}})},
    }.get<WsUserNonFundingLedgerUpdates>();
    assert(ledger.non_funding_ledger_updates.size() == 1);
    assert(std::holds_alternative<WsDeposit>(ledger.non_funding_ledger_updates[0].delta));
    ++checks_run;

    // Real spotTransfer deltas carry destinationDex, feeToken and
    // nativeTokenFee, none of which the docs list. They must be ignored.
    const auto with_extras = json{
        {"time", 2}, {"hash", "0xdef"},
        {"delta", {{"type", "spotTransfer"}, {"token", "USDC"}, {"amount", "1916.73"},
                   {"usdcValue", "1916.73"}, {"user", "0xabc"}, {"destination", "0xdef"},
                   {"fee", "1.0"}, {"destinationDex", ""}, {"feeToken", "USDC"},
                   {"nativeTokenFee", "0.0"}}},
    }.get<WsUserNonFundingLedgerUpdate>();
    assert(near(std::get<WsSpotTransfer>(with_extras.delta).amount, 1916.73));
    ++checks_run;
}

void testWebData3AndOutcomes() {
    // Matches the live shape: no top-level "user" — the address is nested under
    // userState, which is why webData3 cannot be routed on a flat user field.
    const auto web = json{
        {"userState", {{"agentAddress", nullptr}, {"agentValidUntil", nullptr},
                       {"serverTime", 999}, {"cumLedger", "1000.0"},
                       {"isVault", false}, {"user", "0xabc"},
                       {"abstraction", "disabled"}}},  // undocumented, ignored
        {"perpDexStates", json::array({{{"totalVaultEquity", "50.0"}},
                                       {{"totalVaultEquity", "0.0"},
                                        {"perpsAtOpenInterestCap",
                                         json::array({"CANTO", "JELLY"})}}})},
    }.get<WebData3>();
    assert(web.perp_dex_states.size() == 2);
    assert(web.perp_dex_states[1].perps_at_open_interest_cap.has_value());
    assert(web.perp_dex_states[1].perps_at_open_interest_cap->size() == 2);
    // An explicit null reads the same as an absent field.
    assert(!web.user_state.agent_address.has_value());
    assert(!web.user_state.agent_valid_until.has_value());
    assert(!web.user_state.opt_out_of_spot_dusting.has_value());
    assert(web.user_state.server_time == 999);
    assert(near(web.user_state.cum_ledger, 1000.0));
    assert(!web.perp_dex_states[0].leading_vaults.has_value());
    ++checks_run;

    const auto created = json{
        {"outcomeCreated", {{"outcome", 1}, {"name", "Q"}, {"description", "d"},
                            {"sideSpecs", json::array({{{"name", "yes"}}, {{"name", "no"}}})}}},
    }.get<WsOutcomeMetaUpdate>();
    assert(created.outcome_created.has_value());
    assert(created.outcome_created->side_specs[0].name == "yes");
    assert(!created.outcome_settled.has_value());
    ++checks_run;

    const auto settled = json{{"outcomeSettled", 5}}.get<WsOutcomeMetaUpdate>();
    assert(settled.outcome_settled.has_value() && *settled.outcome_settled == 5);
    assert(!settled.outcome_created.has_value());
    ++checks_run;
}

// Matches what decodeFastAssetCtxs() returns: after the snapshot, only changed
// coins appear, and within a coin only changed fields.
void testFastAssetCtxsPartialUpdates() {
    const auto ctxs = json{
        {"BTC", {{"markPx", "97500.5"}}},
        {"ETH", {{"markPx", "3650.25"}, {"midPx", "3650.5"}}},
        {"SOL", json::object()},
        {"DOGE", {{"markPx", "0.1"}, {"midPx", nullptr}}},
    }.get<WsFastAssetCtxs>();

    assert(ctxs.size() == 4);
    assert(near(*ctxs.at("BTC").mark_px, 97500.5));
    assert(!ctxs.at("BTC").mid_px.has_value());   // field omitted
    assert(near(*ctxs.at("ETH").mid_px, 3650.5));
    assert(!ctxs.at("SOL").mark_px.has_value());  // coin present, nothing changed
    assert(!ctxs.at("DOGE").mid_px.has_value());  // explicit null
    ++checks_run;
}

void testMissingRequiredFieldThrows() {
    bool threw = false;
    try {
        (void)json{{"coin", "ETH"}}.get<WsBook>();  // no levels, no time
    } catch (const json::exception&) {
        threw = true;
    }
    assert(threw && "a missing required field must throw");
    ++checks_run;

    // A non-numeric string in a numeric field must throw rather than yield 0.
    threw = false;
    try {
        json bad{{"funding", "abc"}, {"openInterest", "1"}, {"prevDayPx", "1"},
                 {"dayNtlVlm", "1"}, {"oraclePx", "1"}, {"markPx", "1"}};
        (void)bad.get<PerpsAssetCtx>();
    } catch (const json::exception&) {
        threw = true;
    }
    assert(threw && "a non-numeric string must throw, not silently become 0");
    ++checks_run;
}

} // namespace

int main() {
    testMarketDataFromRealShapes();
    testBboHandlesEmptySide();
    testAssetCtxAcceptsStringNumbers();
    testClearinghouseStateAcceptsStringNumbers();
    testUserEventPopulatesOneMember();
    testOrderUpdate();
    testLedgerUpdateVariantDispatch();
    testTwapTypes();
    testSpotAndAllDexsState();
    testUndocumentedEnvelopes();
    testWebData3AndOutcomes();
    testFastAssetCtxsPartialUpdates();
    testMissingRequiredFieldThrows();

    std::cout << "websocket message types: " << checks_run << " checks passed\n";
    return 0;
}
