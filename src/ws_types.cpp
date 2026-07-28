#include "hyperliquid/ws_types.hpp"

#include <stdexcept>

namespace hyperliquid {
namespace {

using nlohmann::json;

/**
 * Read a field the docs declare as `number`.
 *
 * The live API sends prices and quantities as decimal strings even where the
 * TypeScript definitions say number (verified against markPx, accountValue,
 * candle OHLCV and others), so both forms are accepted. A strict get<double>()
 * would throw on every real payload.
 */
double asDouble(const json& value) {
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        const std::string& text = value.get_ref<const std::string&>();
        try {
            return std::stod(text);
        } catch (const std::exception&) {
            throw json::type_error::create(302, "expected a numeric string, got \"" + text + "\"",
                                           &value);
        }
    }
    throw json::type_error::create(302, "expected a number or numeric string", &value);
}

/** asDouble() applied to a required field. */
double number(const json& j, const char* key) {
    return asDouble(j.at(key));
}

/** Reads an optional field; treats an explicit null the same as absent. */
template <typename T>
void optionalField(const json& j, const char* key, std::optional<T>& out) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        out.reset();
        return;
    }
    out = it->get<T>();
}

/** optionalField() for a numeric field that may arrive as a string. */
void optionalNumber(const json& j, const char* key, std::optional<double>& out) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) {
        out.reset();
        return;
    }
    out = asDouble(*it);
}

} // namespace

// --- Market data -----------------------------------------------------------

void from_json(const json& j, WsLevel& out) {
    j.at("px").get_to(out.px);
    j.at("sz").get_to(out.sz);
    j.at("n").get_to(out.n);
}

void from_json(const json& j, WsBook& out) {
    j.at("coin").get_to(out.coin);
    j.at("levels").get_to(out.levels);
    j.at("time").get_to(out.time);
}

void from_json(const json& j, WsBbo& out) {
    j.at("coin").get_to(out.coin);
    j.at("time").get_to(out.time);

    // Either side is null when that side of the book is empty, so this cannot
    // go through the generic array conversion.
    const auto& sides = j.at("bbo");
    for (std::size_t side = 0; side < out.bbo.size(); ++side) {
        if (side < sides.size() && !sides[side].is_null()) {
            out.bbo[side] = sides[side].get<WsLevel>();
        } else {
            out.bbo[side].reset();
        }
    }
}

void from_json(const json& j, WsTrade& out) {
    j.at("coin").get_to(out.coin);
    j.at("side").get_to(out.side);
    j.at("px").get_to(out.px);
    j.at("sz").get_to(out.sz);
    j.at("hash").get_to(out.hash);
    j.at("time").get_to(out.time);
    j.at("tid").get_to(out.tid);
    j.at("users").get_to(out.users);
}

void from_json(const json& j, AllMids& out) {
    j.at("mids").get_to(out.mids);
}

void from_json(const json& j, Candle& out) {
    j.at("t").get_to(out.open_millis);
    j.at("T").get_to(out.close_millis);
    j.at("s").get_to(out.coin);
    j.at("i").get_to(out.interval);
    out.open = number(j, "o");
    out.close = number(j, "c");
    out.high = number(j, "h");
    out.low = number(j, "l");
    out.volume = number(j, "v");
    j.at("n").get_to(out.num_trades);
}

void from_json(const json& j, Notification& out) {
    j.at("notification").get_to(out.notification);
}

// --- User events -----------------------------------------------------------

void from_json(const json& j, FillLiquidation& out) {
    optionalField(j, "liquidatedUser", out.liquidated_user);
    out.mark_px = number(j, "markPx");
    j.at("method").get_to(out.method);
}

void from_json(const json& j, WsFill& out) {
    j.at("coin").get_to(out.coin);
    j.at("px").get_to(out.px);
    j.at("sz").get_to(out.sz);
    j.at("side").get_to(out.side);
    j.at("time").get_to(out.time);
    j.at("startPosition").get_to(out.start_position);
    j.at("dir").get_to(out.dir);
    j.at("closedPnl").get_to(out.closed_pnl);
    j.at("hash").get_to(out.hash);
    j.at("oid").get_to(out.oid);
    j.at("crossed").get_to(out.crossed);
    j.at("fee").get_to(out.fee);
    j.at("tid").get_to(out.tid);
    optionalField(j, "liquidation", out.liquidation);
    j.at("feeToken").get_to(out.fee_token);
    optionalField(j, "builderFee", out.builder_fee);
}

void from_json(const json& j, WsUserFunding& out) {
    j.at("time").get_to(out.time);
    j.at("coin").get_to(out.coin);
    j.at("usdc").get_to(out.usdc);
    j.at("szi").get_to(out.szi);
    j.at("fundingRate").get_to(out.funding_rate);
}

void from_json(const json& j, WsLiquidation& out) {
    j.at("lid").get_to(out.lid);
    j.at("liquidator").get_to(out.liquidator);
    j.at("liquidated_user").get_to(out.liquidated_user);
    j.at("liquidated_ntl_pos").get_to(out.liquidated_ntl_pos);
    j.at("liquidated_account_value").get_to(out.liquidated_account_value);
}

void from_json(const json& j, WsNonUserCancel& out) {
    j.at("coin").get_to(out.coin);
    j.at("oid").get_to(out.oid);
}

void from_json(const json& j, WsUserEvent& out) {
    optionalField(j, "fills", out.fills);
    optionalField(j, "funding", out.funding);
    optionalField(j, "liquidation", out.liquidation);
    optionalField(j, "nonUserCancel", out.non_user_cancel);
}

void from_json(const json& j, WsUserFills& out) {
    optionalField(j, "isSnapshot", out.is_snapshot);
    j.at("user").get_to(out.user);
    j.at("fills").get_to(out.fills);
}

void from_json(const json& j, WsUserFundings& out) {
    optionalField(j, "isSnapshot", out.is_snapshot);
    j.at("user").get_to(out.user);
    j.at("fundings").get_to(out.fundings);
}

// --- Orders ----------------------------------------------------------------

void from_json(const json& j, WsBasicOrder& out) {
    j.at("coin").get_to(out.coin);
    j.at("side").get_to(out.side);
    j.at("limitPx").get_to(out.limit_px);
    j.at("sz").get_to(out.sz);
    j.at("oid").get_to(out.oid);
    j.at("timestamp").get_to(out.timestamp);
    j.at("origSz").get_to(out.orig_sz);
    optionalField(j, "cloid", out.cloid);
}

void from_json(const json& j, WsOrder& out) {
    j.at("order").get_to(out.order);
    j.at("status").get_to(out.status);
    j.at("statusTimestamp").get_to(out.status_timestamp);
}

// --- Asset contexts --------------------------------------------------------

void from_json(const json& j, PerpsAssetCtx& out) {
    out.day_ntl_vlm = number(j, "dayNtlVlm");
    out.prev_day_px = number(j, "prevDayPx");
    out.mark_px = number(j, "markPx");
    optionalNumber(j, "midPx", out.mid_px);
    out.funding = number(j, "funding");
    out.open_interest = number(j, "openInterest");
    out.oracle_px = number(j, "oraclePx");
}

void from_json(const json& j, SpotAssetCtx& out) {
    out.day_ntl_vlm = number(j, "dayNtlVlm");
    out.prev_day_px = number(j, "prevDayPx");
    out.mark_px = number(j, "markPx");
    optionalNumber(j, "midPx", out.mid_px);
    out.circulating_supply = number(j, "circulatingSupply");
}

void from_json(const json& j, WsActiveAssetCtx& out) {
    j.at("coin").get_to(out.coin);
    j.at("ctx").get_to(out.ctx);
}

void from_json(const json& j, WsActiveSpotAssetCtx& out) {
    j.at("coin").get_to(out.coin);
    j.at("ctx").get_to(out.ctx);
}

void from_json(const json& j, Leverage& out) {
    j.at("type").get_to(out.type);
    out.value = number(j, "value");
    optionalField(j, "rawUsd", out.raw_usd);
}

void from_json(const json& j, WsActiveAssetData& out) {
    j.at("user").get_to(out.user);
    j.at("coin").get_to(out.coin);
    j.at("leverage").get_to(out.leverage);

    const auto& max_szs = j.at("maxTradeSzs");
    const auto& available = j.at("availableToTrade");
    for (std::size_t i = 0; i < out.max_trade_szs.size(); ++i) {
        out.max_trade_szs[i] = asDouble(max_szs.at(i));
        out.available_to_trade[i] = asDouble(available.at(i));
    }
}

// --- TWAP ------------------------------------------------------------------

void from_json(const json& j, TwapState& out) {
    j.at("coin").get_to(out.coin);
    j.at("user").get_to(out.user);
    j.at("side").get_to(out.side);
    out.sz = number(j, "sz");
    out.executed_sz = number(j, "executedSz");
    out.executed_ntl = number(j, "executedNtl");
    j.at("minutes").get_to(out.minutes);
    j.at("reduceOnly").get_to(out.reduce_only);
    j.at("randomize").get_to(out.randomize);
    j.at("timestamp").get_to(out.timestamp);
}

void from_json(const json& j, WsTwapSliceFill& out) {
    j.at("fill").get_to(out.fill);
    j.at("twapId").get_to(out.twap_id);
}

void from_json(const json& j, WsUserTwapSliceFills& out) {
    optionalField(j, "isSnapshot", out.is_snapshot);
    j.at("user").get_to(out.user);
    j.at("twapSliceFills").get_to(out.twap_slice_fills);
}

void from_json(const json& j, WsTwapHistory& out) {
    j.at("state").get_to(out.state);
    // Flattened: the wire nests status/description one level deeper, but a
    // two-field struct for it would only add indirection. `description` is
    // optional in practice — live payloads send {"status": "activated"} alone,
    // so requiring it threw on every real message.
    const auto& status = j.at("status");
    status.at("status").get_to(out.status);
    optionalField(status, "description", out.status_description);
    j.at("time").get_to(out.time);
    optionalField(j, "twapId", out.twap_id);
}

void from_json(const json& j, WsUserTwapHistory& out) {
    optionalField(j, "isSnapshot", out.is_snapshot);
    j.at("user").get_to(out.user);
    j.at("history").get_to(out.history);
}

void from_json(const json& j, TwapStates& out) {
    j.at("dex").get_to(out.dex);
    j.at("user").get_to(out.user);
    j.at("states").get_to(out.states);
}

// --- webData3 --------------------------------------------------------------

void from_json(const json& j, LeadingVault& out) {
    j.at("address").get_to(out.address);
    j.at("name").get_to(out.name);
}

void from_json(const json& j, PerpDexState& out) {
    out.total_vault_equity = number(j, "totalVaultEquity");
    optionalField(j, "perpsAtOpenInterestCap", out.perps_at_open_interest_cap);
    optionalField(j, "leadingVaults", out.leading_vaults);
}

void from_json(const json& j, WebData3UserState& out) {
    optionalField(j, "agentAddress", out.agent_address);
    optionalField(j, "agentValidUntil", out.agent_valid_until);
    j.at("serverTime").get_to(out.server_time);
    out.cum_ledger = number(j, "cumLedger");
    j.at("isVault").get_to(out.is_vault);
    j.at("user").get_to(out.user);
    optionalField(j, "optOutOfSpotDusting", out.opt_out_of_spot_dusting);
    optionalField(j, "dexAbstractionEnabled", out.dex_abstraction_enabled);
}

void from_json(const json& j, WebData3& out) {
    j.at("userState").get_to(out.user_state);
    j.at("perpDexStates").get_to(out.perp_dex_states);
}

// --- Clearinghouse and spot state ------------------------------------------

void from_json(const json& j, MarginSummary& out) {
    out.account_value = number(j, "accountValue");
    out.total_ntl_pos = number(j, "totalNtlPos");
    out.total_raw_usd = number(j, "totalRawUsd");
    out.total_margin_used = number(j, "totalMarginUsed");
}

void from_json(const json& j, AssetPosition& out) {
    j.at("type").get_to(out.type);
    out.position = j.at("position");
}

void from_json(const json& j, InnerClearinghouseState& out) {
    j.at("assetPositions").get_to(out.asset_positions);
    j.at("marginSummary").get_to(out.margin_summary);
    j.at("crossMarginSummary").get_to(out.cross_margin_summary);
    out.cross_maintenance_margin_used = number(j, "crossMaintenanceMarginUsed");
    out.withdrawable = number(j, "withdrawable");
}

void from_json(const json& j, ClearinghouseState& out) {
    j.at("dex").get_to(out.dex);
    j.at("user").get_to(out.user);
    j.at("clearinghouseState").get_to(out.clearinghouse_state);
}

void from_json(const json& j, OpenOrders& out) {
    j.at("dex").get_to(out.dex);
    j.at("user").get_to(out.user);
    out.orders = j.at("orders");
}

void from_json(const json& j, UserBalance& out) {
    j.at("coin").get_to(out.coin);
    j.at("token").get_to(out.token);
    j.at("hold").get_to(out.hold);
    j.at("total").get_to(out.total);
    j.at("entryNtl").get_to(out.entry_ntl);
}

void from_json(const json& j, SpotState& out) {
    j.at("balances").get_to(out.balances);
}

void from_json(const json& j, WsSpotState& out) {
    j.at("user").get_to(out.user);
    j.at("spotState").get_to(out.spot_state);
}

void from_json(const json& j, WsAllDexsClearinghouseState& out) {
    j.at("user").get_to(out.user);
    j.at("clearinghouseStates").get_to(out.clearinghouse_states);
}

void from_json(const json& j, WsAllDexsAssetCtxs& out) {
    j.at("ctxs").get_to(out.ctxs);
}

// --- Prediction market outcomes --------------------------------------------

void from_json(const json& j, OutcomeSideSpec& out) {
    j.at("name").get_to(out.name);
}

void from_json(const json& j, OutcomeSpec& out) {
    j.at("outcome").get_to(out.outcome);
    j.at("name").get_to(out.name);
    j.at("description").get_to(out.description);
    j.at("sideSpecs").get_to(out.side_specs);
}

void from_json(const json& j, QuestionSpec& out) {
    j.at("question").get_to(out.question);
    j.at("name").get_to(out.name);
    j.at("description").get_to(out.description);
    j.at("fallbackOutcome").get_to(out.fallback_outcome);
    j.at("namedOutcomes").get_to(out.named_outcomes);
    j.at("settledNamedOutcomes").get_to(out.settled_named_outcomes);
}

void from_json(const json& j, WsOutcomeMetaUpdate& out) {
    optionalField(j, "outcomeCreated", out.outcome_created);
    optionalField(j, "outcomeSettled", out.outcome_settled);
    optionalField(j, "questionUpdated", out.question_updated);
    optionalField(j, "questionSettled", out.question_settled);
}

// --- fastAssetCtxs ---------------------------------------------------------

void from_json(const json& j, FastAssetCtx& out) {
    optionalNumber(j, "markPx", out.mark_px);
    optionalNumber(j, "midPx", out.mid_px);
}

// --- Non-funding ledger updates --------------------------------------------

void from_json(const json& j, WsDeposit& out) {
    out.usdc = number(j, "usdc");
}

void from_json(const json& j, WsWithdraw& out) {
    out.usdc = number(j, "usdc");
    j.at("nonce").get_to(out.nonce);
    out.fee = number(j, "fee");
}

void from_json(const json& j, WsInternalTransfer& out) {
    out.usdc = number(j, "usdc");
    j.at("user").get_to(out.user);
    j.at("destination").get_to(out.destination);
    out.fee = number(j, "fee");
}

void from_json(const json& j, WsSubAccountTransfer& out) {
    out.usdc = number(j, "usdc");
    j.at("user").get_to(out.user);
    j.at("destination").get_to(out.destination);
}

void from_json(const json& j, LiquidatedPosition& out) {
    j.at("coin").get_to(out.coin);
    out.szi = number(j, "szi");
}

void from_json(const json& j, WsLedgerLiquidation& out) {
    out.account_value = number(j, "accountValue");
    j.at("leverageType").get_to(out.leverage_type);
    j.at("liquidatedPositions").get_to(out.liquidated_positions);
}

void from_json(const json& j, WsVaultDelta& out) {
    j.at("type").get_to(out.type);
    j.at("vault").get_to(out.vault);
    out.usdc = number(j, "usdc");
}

void from_json(const json& j, WsVaultWithdrawal& out) {
    j.at("vault").get_to(out.vault);
    j.at("user").get_to(out.user);
    out.requested_usd = number(j, "requestedUsd");
    out.commission = number(j, "commission");
    out.closing_cost = number(j, "closingCost");
    out.basis = number(j, "basis");
    out.net_withdrawn_usd = number(j, "netWithdrawnUsd");
}

void from_json(const json& j, WsVaultLeaderCommission& out) {
    j.at("user").get_to(out.user);
    out.usdc = number(j, "usdc");
}

void from_json(const json& j, WsSpotTransfer& out) {
    j.at("token").get_to(out.token);
    out.amount = number(j, "amount");
    out.usdc_value = number(j, "usdcValue");
    j.at("user").get_to(out.user);
    j.at("destination").get_to(out.destination);
    out.fee = number(j, "fee");
}

void from_json(const json& j, WsAccountClassTransfer& out) {
    out.usdc = number(j, "usdc");
    j.at("toPerp").get_to(out.to_perp);
}

void from_json(const json& j, WsSpotGenesis& out) {
    j.at("token").get_to(out.token);
    out.amount = number(j, "amount");
}

void from_json(const json& j, WsRewardsClaim& out) {
    out.amount = number(j, "amount");
}

void from_json(const json& j, WsSend& out) {
    j.at("token").get_to(out.token);
    out.amount = number(j, "amount");
    out.usdc_value = number(j, "usdcValue");
    j.at("user").get_to(out.user);
    j.at("destination").get_to(out.destination);
    out.fee = number(j, "fee");
    j.at("feeToken").get_to(out.fee_token);
    out.native_token_fee = number(j, "nativeTokenFee");
    j.at("nonce").get_to(out.nonce);
    j.at("sourceDex").get_to(out.source_dex);
    j.at("destinationDex").get_to(out.destination_dex);
}

void from_json(const json& j, WsUserNonFundingLedgerUpdate& out) {
    j.at("time").get_to(out.time);
    j.at("hash").get_to(out.hash);

    // Tagged union: "type" selects the alternative. The three vault deltas share
    // one struct because they share one layout and differ only by that tag.
    const auto& delta = j.at("delta");
    const std::string type = delta.at("type").get<std::string>();
    if (type == "deposit") {
        out.delta = delta.get<WsDeposit>();
    } else if (type == "withdraw") {
        out.delta = delta.get<WsWithdraw>();
    } else if (type == "internalTransfer") {
        out.delta = delta.get<WsInternalTransfer>();
    } else if (type == "subAccountTransfer") {
        out.delta = delta.get<WsSubAccountTransfer>();
    } else if (type == "liquidation") {
        out.delta = delta.get<WsLedgerLiquidation>();
    } else if (type == "vaultCreate" || type == "vaultDeposit" || type == "vaultDistribution") {
        out.delta = delta.get<WsVaultDelta>();
    } else if (type == "vaultWithdraw") {
        out.delta = delta.get<WsVaultWithdrawal>();
    } else if (type == "vaultLeaderCommission") {
        out.delta = delta.get<WsVaultLeaderCommission>();
    } else if (type == "spotTransfer") {
        out.delta = delta.get<WsSpotTransfer>();
    } else if (type == "accountClassTransfer") {
        out.delta = delta.get<WsAccountClassTransfer>();
    } else if (type == "spotGenesis") {
        out.delta = delta.get<WsSpotGenesis>();
    } else if (type == "rewardsClaim") {
        out.delta = delta.get<WsRewardsClaim>();
    } else if (type == "send") {
        out.delta = delta.get<WsSend>();
    } else {
        // A variant this SDK does not model. `send` proved the documented list
        // is already incomplete, so throwing here would let one unrecognised
        // entry make an entire account's feed unreadable. Hand it back intact
        // and clearly labelled instead.
        out.delta = WsUnknownDelta{type, delta};
    }
}

void from_json(const json& j, WsUserNonFundingLedgerUpdates& out) {
    optionalField(j, "isSnapshot", out.is_snapshot);
    j.at("user").get_to(out.user);
    j.at("nonFundingLedgerUpdates").get_to(out.non_funding_ledger_updates);
}

} // namespace hyperliquid
