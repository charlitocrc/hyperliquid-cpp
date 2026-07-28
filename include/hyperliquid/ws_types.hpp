#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>

// Typed views over websocket message payloads.
//
// These are strictly opt-in. WebSocketManager delivers raw nlohmann::json to
// callbacks (see CLAUDE.md §5), and nothing here changes that. Convert only the
// payloads you care about:
//
//     ws.subscribe(json{{"type","l2Book"},{"coin","ETH"}}, [](const json& msg) {
//         const auto book = msg["data"].get<hyperliquid::WsBook>();
//         use(book.levels[0].front().px);
//     });
//
// Conversion throws nlohmann::json::exception when a payload lacks a required
// field or has an unexpected shape. Inside a subscription callback that
// exception is caught and the message is dropped, so wrap conversions in
// try/catch if you need to see the failure.
//
// Field names and layouts come from the "Data type definitions" block of
// https://hyperliquid.gitbook.io/hyperliquid-docs/for-developers/api/websocket/subscriptions
//
// ONE IMPORTANT DEVIATION FROM THOSE DOCS: every price and quantity the docs
// declare as `number` is sent as a decimal string on the live wire. Verified
// against the API: markPx, midPx, funding, openInterest, oraclePx, dayNtlVlm
// and prevDayPx in the asset contexts; accountValue, totalNtlPos, totalRawUsd,
// totalMarginUsed and withdrawable in the clearinghouse state; o/c/h/l/v in
// candles. Every such field is read here through a converter that accepts a
// JSON number or a numeric string, so both forms work. Fields the docs declare
// as `string` stay std::string, which also keeps exact decimal precision on the
// money-carrying fill and order fields.

namespace hyperliquid {

// ---------------------------------------------------------------------------
// Market data
// ---------------------------------------------------------------------------

/** One aggregated price level of the order book. */
struct WsLevel {
    std::string px;  // price
    std::string sz;  // size
    int n = 0;       // number of orders at this level
};

/**
 * Order book snapshot. Pushed per block, at most every 0.5s.
 * levels[0] is the bid side, levels[1] the ask side.
 */
struct WsBook {
    std::string coin;
    std::array<std::vector<WsLevel>, 2> levels;
    int64_t time = 0;  // millis
};

/** Best bid/offer, pushed only when the bbo changes on a block. */
struct WsBbo {
    std::string coin;
    int64_t time = 0;  // millis
    // Either side is absent when that side of the book is empty.
    std::array<std::optional<WsLevel>, 2> bbo;
};

/** A single trade print. */
struct WsTrade {
    std::string coin;
    std::string side;
    std::string px;
    std::string sz;
    std::string hash;
    int64_t time = 0;
    // 50-bit hash of (buyer_oid, seller_oid). Globally unique only in
    // combination with block time and coin, per the docs.
    int64_t tid = 0;
    std::array<std::string, 2> users;  // [buyer, seller]
};

/** All mid prices, keyed by coin. */
struct AllMids {
    std::map<std::string, std::string> mids;
};

/** OHLCV bar. o/c/h/l/v are documented as numbers but arrive as strings. */
struct Candle {
    int64_t open_millis = 0;   // "t"
    int64_t close_millis = 0;  // "T"
    std::string coin;          // "s"
    std::string interval;      // "i"
    double open = 0.0;         // "o"
    double close = 0.0;        // "c"
    double high = 0.0;         // "h"
    double low = 0.0;          // "l"
    double volume = 0.0;       // "v", in base units
    int num_trades = 0;        // "n"
};

/** A free-text notification for a user. */
struct Notification {
    std::string notification;
};

// ---------------------------------------------------------------------------
// User events
// ---------------------------------------------------------------------------

/** Present on a fill that resulted from a liquidation. */
struct FillLiquidation {
    std::optional<std::string> liquidated_user;
    double mark_px = 0.0;
    std::string method;  // "market" or "backstop"
};

/** A single fill against one of the user's orders. */
struct WsFill {
    std::string coin;
    std::string px;
    std::string sz;
    std::string side;
    int64_t time = 0;
    std::string start_position;
    std::string dir;  // frontend display string
    std::string closed_pnl;
    std::string hash;  // L1 transaction hash
    int64_t oid = 0;
    bool crossed = false;  // true when this order was the taker
    std::string fee;       // negative means rebate
    int64_t tid = 0;
    std::optional<FillLiquidation> liquidation;
    std::string fee_token;
    std::optional<std::string> builder_fee;  // included in `fee`, not additional
};

/**
 * An hourly funding payment.
 *
 * Live payloads also carry an `nSamples` field that the docs omit; it is
 * ignored here, as are all unrecognised fields.
 */
struct WsUserFunding {
    int64_t time = 0;
    std::string coin;
    std::string usdc;
    std::string szi;
    std::string funding_rate;
};

/** A liquidation event against the user. */
struct WsLiquidation {
    int64_t lid = 0;
    std::string liquidator;
    std::string liquidated_user;
    std::string liquidated_ntl_pos;
    std::string liquidated_account_value;
};

/** An order cancelled by the system rather than by the user. */
struct WsNonUserCancel {
    std::string coin;
    int64_t oid = 0;
};

/**
 * A userEvents payload. The wire form is an object carrying exactly one of
 * these keys, so exactly one member is populated. Checked with `if (event.fills)`
 * rather than a std::variant because the union is expressed by key presence,
 * not by a discriminator field.
 */
struct WsUserEvent {
    std::optional<std::vector<WsFill>> fills;
    std::optional<WsUserFunding> funding;
    std::optional<WsLiquidation> liquidation;
    std::optional<std::vector<WsNonUserCancel>> non_user_cancel;
};

/** Fills snapshot, then streaming fills. is_snapshot is true on the first only. */
struct WsUserFills {
    std::optional<bool> is_snapshot;
    std::string user;
    std::vector<WsFill> fills;
};

/**
 * userFundings envelope.
 *
 * The docs name this type without defining its shape; the field names here were
 * read off a live payload: {"fundings": [...], "isSnapshot": true, "user": ...}.
 */
struct WsUserFundings {
    std::optional<bool> is_snapshot;
    std::string user;
    std::vector<WsUserFunding> fundings;
};

// ---------------------------------------------------------------------------
// Orders
// ---------------------------------------------------------------------------

/** The order an update refers to. */
struct WsBasicOrder {
    std::string coin;
    std::string side;
    std::string limit_px;
    std::string sz;
    int64_t oid = 0;
    int64_t timestamp = 0;
    std::string orig_sz;
    std::optional<std::string> cloid;
};

/** A status transition on one of the user's orders. */
struct WsOrder {
    WsBasicOrder order;
    // See the info endpoint's "order status" docs for the full value list.
    std::string status;
    int64_t status_timestamp = 0;
};

// ---------------------------------------------------------------------------
// Asset contexts
// ---------------------------------------------------------------------------

/** Perp market context. All fields arrive as decimal strings. */
struct PerpsAssetCtx {
    double day_ntl_vlm = 0.0;
    double prev_day_px = 0.0;
    double mark_px = 0.0;
    std::optional<double> mid_px;  // absent when the book is one-sided
    double funding = 0.0;
    double open_interest = 0.0;
    double oracle_px = 0.0;
};

/** Spot market context. */
struct SpotAssetCtx {
    double day_ntl_vlm = 0.0;
    double prev_day_px = 0.0;
    double mark_px = 0.0;
    std::optional<double> mid_px;
    double circulating_supply = 0.0;
};

/** activeAssetCtx payload for a perp coin (channel "activeAssetCtx"). */
struct WsActiveAssetCtx {
    std::string coin;
    PerpsAssetCtx ctx;
};

/** activeAssetCtx payload for a spot coin (channel "activeSpotAssetCtx"). */
struct WsActiveSpotAssetCtx {
    std::string coin;
    SpotAssetCtx ctx;
};

/**
 * Position leverage.
 *
 * The websocket docs reference this type but never define it; the layout was
 * taken from the info endpoint and confirmed against a live position, which
 * returns {"type":"cross","value":20}. `raw_usd` is absent for cross leverage.
 */
struct Leverage {
    std::string type;  // "cross" or "isolated"
    double value = 0.0;
    std::optional<std::string> raw_usd;
};

/** Per-asset trading limits for a user. Perps only. */
struct WsActiveAssetData {
    std::string user;
    std::string coin;
    Leverage leverage;
    std::array<double, 2> max_trade_szs{};
    std::array<double, 2> available_to_trade{};
};

// ---------------------------------------------------------------------------
// TWAP
// ---------------------------------------------------------------------------

/** The state of one running or finished TWAP. */
struct TwapState {
    std::string coin;
    std::string user;
    std::string side;
    double sz = 0.0;
    double executed_sz = 0.0;
    double executed_ntl = 0.0;
    int minutes = 0;
    bool reduce_only = false;
    bool randomize = false;
    int64_t timestamp = 0;
};

/** One 30-second TWAP slice that filled. */
struct WsTwapSliceFill {
    WsFill fill;
    int64_t twap_id = 0;
};

struct WsUserTwapSliceFills {
    std::optional<bool> is_snapshot;
    std::string user;
    std::vector<WsTwapSliceFill> twap_slice_fills;
};

/**
 * A TWAP status transition. status is one of activated/terminated/finished/error.
 *
 * status_description is optional even though the docs mark it required: live
 * payloads send `"status": {"status": "activated"}` with no description at all.
 * twap_id is not in the docs but is present on the wire and is the only way to
 * tie an entry back to a running TWAP.
 */
struct WsTwapHistory {
    TwapState state;
    std::string status;
    std::optional<std::string> status_description;
    int64_t time = 0;
    std::optional<int64_t> twap_id;
};

struct WsUserTwapHistory {
    std::optional<bool> is_snapshot;
    std::string user;
    std::vector<WsTwapHistory> history;
};

/** twapStates payload: running TWAPs for a user on one dex, keyed by twap id. */
struct TwapStates {
    std::string dex;
    std::string user;
    std::vector<std::pair<int64_t, TwapState>> states;
};

// ---------------------------------------------------------------------------
// webData3
// ---------------------------------------------------------------------------

struct LeadingVault {
    std::string address;
    std::string name;
};

struct PerpDexState {
    double total_vault_equity = 0.0;
    std::optional<std::vector<std::string>> perps_at_open_interest_cap;
    std::optional<std::vector<LeadingVault>> leading_vaults;
};

struct WebData3UserState {
    std::optional<std::string> agent_address;
    std::optional<int64_t> agent_valid_until;
    int64_t server_time = 0;
    double cum_ledger = 0.0;
    bool is_vault = false;
    std::string user;
    std::optional<bool> opt_out_of_spot_dusting;
    std::optional<bool> dex_abstraction_enabled;
};

/**
 * Aggregate user information for frontends. The docs warn that undocumented
 * extra fields exist today and will be removed later, so only the documented
 * ones are modelled here; read msg["data"] directly for the rest.
 */
struct WebData3 {
    WebData3UserState user_state;
    std::vector<PerpDexState> perp_dex_states;
};

// ---------------------------------------------------------------------------
// Clearinghouse and spot state
// ---------------------------------------------------------------------------

/** Account totals. Documented as numbers, sent as decimal strings. */
struct MarginSummary {
    double account_value = 0.0;
    double total_ntl_pos = 0.0;
    double total_raw_usd = 0.0;
    double total_margin_used = 0.0;
};

/**
 * One open position.
 *
 * `position` stays raw JSON: the websocket docs name the inner `Position` type
 * but never define its fields, and inventing a layout from another endpoint
 * risks being silently wrong.
 */
struct AssetPosition {
    std::string type;  // "oneWay"
    nlohmann::json position;
};

struct InnerClearinghouseState {
    std::vector<AssetPosition> asset_positions;
    MarginSummary margin_summary;
    MarginSummary cross_margin_summary;
    double cross_maintenance_margin_used = 0.0;
    double withdrawable = 0.0;
};

struct ClearinghouseState {
    std::string dex;
    std::string user;
    InnerClearinghouseState clearinghouse_state;
};

/**
 * Open orders for a user on one dex.
 *
 * `orders` stays raw JSON for the same reason as AssetPosition::position: the
 * websocket docs reference `Order` without defining it.
 */
struct OpenOrders {
    std::string dex;
    std::string user;
    nlohmann::json orders;
};

struct UserBalance {
    std::string coin;
    int token = 0;
    std::string hold;
    std::string total;
    std::string entry_ntl;
};

struct SpotState {
    std::vector<UserBalance> balances;
};

struct WsSpotState {
    std::string user;
    SpotState spot_state;
};

/** Clearinghouse state on every dex, as (dex name, state) pairs. */
struct WsAllDexsClearinghouseState {
    std::string user;
    std::vector<std::pair<std::string, InnerClearinghouseState>> clearinghouse_states;
};

/** Asset contexts on every dex, as (dex name, contexts) pairs. */
struct WsAllDexsAssetCtxs {
    std::vector<std::pair<std::string, std::vector<PerpsAssetCtx>>> ctxs;
};

// ---------------------------------------------------------------------------
// Prediction market outcomes
// ---------------------------------------------------------------------------

struct OutcomeSideSpec {
    std::string name;
};

struct OutcomeSpec {
    int64_t outcome = 0;
    std::string name;
    std::string description;
    std::array<OutcomeSideSpec, 2> side_specs;
};

struct QuestionSpec {
    int64_t question = 0;
    std::string name;
    std::string description;
    int64_t fallback_outcome = 0;
    std::vector<int64_t> named_outcomes;
    std::vector<int64_t> settled_named_outcomes;
};

/** Exactly one member is populated, as with WsUserEvent. */
struct WsOutcomeMetaUpdate {
    std::optional<OutcomeSpec> outcome_created;
    std::optional<int64_t> outcome_settled;
    std::optional<QuestionSpec> question_updated;
    std::optional<int64_t> question_settled;
};

using WsOutcomeMetaUpdates = std::vector<WsOutcomeMetaUpdate>;

// ---------------------------------------------------------------------------
// fastAssetCtxs
// ---------------------------------------------------------------------------

/**
 * One coin's entry in a fastAssetCtxs message. Both fields are optional: after
 * the opening snapshot, each message carries only the coins that changed, and
 * within a coin only the fields that changed.
 */
struct FastAssetCtx {
    std::optional<double> mark_px;
    std::optional<double> mid_px;  // may also be explicitly null
};

/** Decoded fastAssetCtxs payload, keyed by coin. See decodeFastAssetCtxs(). */
using WsFastAssetCtxs = std::map<std::string, FastAssetCtx>;

// ---------------------------------------------------------------------------
// Non-funding ledger updates
// ---------------------------------------------------------------------------

struct WsDeposit {
    double usdc = 0.0;
};

struct WsWithdraw {
    double usdc = 0.0;
    int64_t nonce = 0;
    double fee = 0.0;
};

struct WsInternalTransfer {
    double usdc = 0.0;
    std::string user;
    std::string destination;
    double fee = 0.0;
};

struct WsSubAccountTransfer {
    double usdc = 0.0;
    std::string user;
    std::string destination;
};

struct LiquidatedPosition {
    std::string coin;
    double szi = 0.0;
};

struct WsLedgerLiquidation {
    // For isolated positions this is the isolated account value, not the
    // account total.
    double account_value = 0.0;
    std::string leverage_type;  // "Cross" or "Isolated"
    std::vector<LiquidatedPosition> liquidated_positions;
};

/** Covers the vaultCreate, vaultDeposit and vaultDistribution deltas. */
struct WsVaultDelta {
    std::string type;
    std::string vault;
    double usdc = 0.0;
};

struct WsVaultWithdrawal {
    std::string vault;
    std::string user;
    double requested_usd = 0.0;
    double commission = 0.0;
    double closing_cost = 0.0;
    double basis = 0.0;
    double net_withdrawn_usd = 0.0;
};

struct WsVaultLeaderCommission {
    std::string user;
    double usdc = 0.0;
};

struct WsSpotTransfer {
    std::string token;
    double amount = 0.0;
    double usdc_value = 0.0;
    std::string user;
    std::string destination;
    double fee = 0.0;
};

struct WsAccountClassTransfer {
    double usdc = 0.0;
    bool to_perp = false;
};

struct WsSpotGenesis {
    std::string token;
    double amount = 0.0;
};

struct WsRewardsClaim {
    double amount = 0.0;
};

/**
 * The ledger entry for a sendAsset action.
 *
 * Not in the docs' list of delta types, but it is what real accounts receive.
 * Distinct from WsSpotTransfer, which it otherwise resembles: this one also
 * carries the source and destination dex and a native-token fee.
 */
struct WsSend {
    std::string token;
    double amount = 0.0;
    double usdc_value = 0.0;
    std::string user;
    std::string destination;
    double fee = 0.0;
    std::string fee_token;
    double native_token_fee = 0.0;
    int64_t nonce = 0;
    std::string source_dex;
    std::string destination_dex;
};

/**
 * A delta whose "type" this SDK does not model.
 *
 * The documented list of variants is already incomplete — `send` was found on a
 * live account and is absent from the docs — so an unrecognised type is a
 * routine occurrence, not a corrupt payload. Surfacing it as an explicit
 * alternative keeps one unknown entry from making a whole feed unreadable,
 * while still being impossible to mistake for a decoded one.
 */
struct WsUnknownDelta {
    std::string type;
    nlohmann::json raw;
};

/**
 * A ledger delta. Unlike WsUserEvent this is a genuinely tagged union — the
 * wire object carries a "type" discriminator — so it maps to std::variant and
 * is consumed with std::visit or std::get_if.
 */
using WsLedgerUpdate = std::variant<WsDeposit,
                                    WsWithdraw,
                                    WsInternalTransfer,
                                    WsSubAccountTransfer,
                                    WsLedgerLiquidation,
                                    WsVaultDelta,
                                    WsVaultWithdrawal,
                                    WsVaultLeaderCommission,
                                    WsSpotTransfer,
                                    WsAccountClassTransfer,
                                    WsSpotGenesis,
                                    WsRewardsClaim,
                                    WsSend,
                                    WsUnknownDelta>;

/** One entry of a userNonFundingLedgerUpdates feed. */
struct WsUserNonFundingLedgerUpdate {
    int64_t time = 0;
    std::string hash;
    WsLedgerUpdate delta;
};

/**
 * userNonFundingLedgerUpdates envelope.
 *
 * As with WsUserFundings the docs never define this wrapper; the field names
 * come from a live payload:
 * {"isSnapshot": true, "nonFundingLedgerUpdates": [...], "user": ...}.
 */
struct WsUserNonFundingLedgerUpdates {
    std::optional<bool> is_snapshot;
    std::string user;
    std::vector<WsUserNonFundingLedgerUpdate> non_funding_ledger_updates;
};

// ---------------------------------------------------------------------------
// JSON conversion (found by ADL; call msg["data"].get<T>())
// ---------------------------------------------------------------------------

void from_json(const nlohmann::json& j, WsLevel& out);
void from_json(const nlohmann::json& j, WsBook& out);
void from_json(const nlohmann::json& j, WsBbo& out);
void from_json(const nlohmann::json& j, WsTrade& out);
void from_json(const nlohmann::json& j, AllMids& out);
void from_json(const nlohmann::json& j, Candle& out);
void from_json(const nlohmann::json& j, Notification& out);

void from_json(const nlohmann::json& j, FillLiquidation& out);
void from_json(const nlohmann::json& j, WsFill& out);
void from_json(const nlohmann::json& j, WsUserFunding& out);
void from_json(const nlohmann::json& j, WsLiquidation& out);
void from_json(const nlohmann::json& j, WsNonUserCancel& out);
void from_json(const nlohmann::json& j, WsUserEvent& out);
void from_json(const nlohmann::json& j, WsUserFills& out);
void from_json(const nlohmann::json& j, WsUserFundings& out);

void from_json(const nlohmann::json& j, WsBasicOrder& out);
void from_json(const nlohmann::json& j, WsOrder& out);

void from_json(const nlohmann::json& j, PerpsAssetCtx& out);
void from_json(const nlohmann::json& j, SpotAssetCtx& out);
void from_json(const nlohmann::json& j, WsActiveAssetCtx& out);
void from_json(const nlohmann::json& j, WsActiveSpotAssetCtx& out);
void from_json(const nlohmann::json& j, Leverage& out);
void from_json(const nlohmann::json& j, WsActiveAssetData& out);

void from_json(const nlohmann::json& j, TwapState& out);
void from_json(const nlohmann::json& j, WsTwapSliceFill& out);
void from_json(const nlohmann::json& j, WsUserTwapSliceFills& out);
void from_json(const nlohmann::json& j, WsTwapHistory& out);
void from_json(const nlohmann::json& j, WsUserTwapHistory& out);
void from_json(const nlohmann::json& j, TwapStates& out);

void from_json(const nlohmann::json& j, LeadingVault& out);
void from_json(const nlohmann::json& j, PerpDexState& out);
void from_json(const nlohmann::json& j, WebData3UserState& out);
void from_json(const nlohmann::json& j, WebData3& out);

void from_json(const nlohmann::json& j, MarginSummary& out);
void from_json(const nlohmann::json& j, AssetPosition& out);
void from_json(const nlohmann::json& j, InnerClearinghouseState& out);
void from_json(const nlohmann::json& j, ClearinghouseState& out);
void from_json(const nlohmann::json& j, OpenOrders& out);
void from_json(const nlohmann::json& j, UserBalance& out);
void from_json(const nlohmann::json& j, SpotState& out);
void from_json(const nlohmann::json& j, WsSpotState& out);
void from_json(const nlohmann::json& j, WsAllDexsClearinghouseState& out);
void from_json(const nlohmann::json& j, WsAllDexsAssetCtxs& out);

void from_json(const nlohmann::json& j, OutcomeSideSpec& out);
void from_json(const nlohmann::json& j, OutcomeSpec& out);
void from_json(const nlohmann::json& j, QuestionSpec& out);
void from_json(const nlohmann::json& j, WsOutcomeMetaUpdate& out);

void from_json(const nlohmann::json& j, FastAssetCtx& out);

void from_json(const nlohmann::json& j, WsDeposit& out);
void from_json(const nlohmann::json& j, WsWithdraw& out);
void from_json(const nlohmann::json& j, WsInternalTransfer& out);
void from_json(const nlohmann::json& j, WsSubAccountTransfer& out);
void from_json(const nlohmann::json& j, LiquidatedPosition& out);
void from_json(const nlohmann::json& j, WsLedgerLiquidation& out);
void from_json(const nlohmann::json& j, WsVaultDelta& out);
void from_json(const nlohmann::json& j, WsVaultWithdrawal& out);
void from_json(const nlohmann::json& j, WsVaultLeaderCommission& out);
void from_json(const nlohmann::json& j, WsSpotTransfer& out);
void from_json(const nlohmann::json& j, WsAccountClassTransfer& out);
void from_json(const nlohmann::json& j, WsSpotGenesis& out);
void from_json(const nlohmann::json& j, WsRewardsClaim& out);
void from_json(const nlohmann::json& j, WsSend& out);
void from_json(const nlohmann::json& j, WsUserNonFundingLedgerUpdate& out);
void from_json(const nlohmann::json& j, WsUserNonFundingLedgerUpdates& out);

} // namespace hyperliquid
