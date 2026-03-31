#pragma once

#include "hyperliquid/api.hpp"
#include "hyperliquid/types.hpp"
#include <unordered_map>
#include <vector>
#include <optional>

namespace hyperliquid {

/**
 * Info class for querying market data and user information
 */
class Info : public API {
public:
    explicit Info(const std::string& base_url = "",
                 bool skip_ws = true,
                 const Meta* meta = nullptr,
                 const SpotMeta* spot_meta = nullptr,
                 const std::vector<std::string>* perp_dexs = nullptr,
                 int timeout_ms = 30000);

    /**
     * Get asset number from coin/pair name
     */
    int nameToAsset(const std::string& name) const;

    /**
     * Get canonical coin name from display name
     */
    const std::string& nameToCoin(const std::string& name) const;

    /**
     * Query user state (positions, margin summary)
     */
    nlohmann::json userState(const std::string& address, const std::string& dex = "");

    /**
     * Query spot user state (balances, spot positions)
     */
    nlohmann::json spotUserState(const std::string& address);

    /**
     * Query user's open orders
     */
    nlohmann::json openOrders(const std::string& address, const std::string& dex = "");

    /**
     * Query user's open orders with additional frontend info
     *
     * Returns extended order information including trigger conditions,
     * order type, reduce-only status, time-in-force, and child orders.
     *
     * @param address Onchain address in 42-character hexadecimal format
     * @param dex Optional dex identifier (empty string for default dex)
     * @return Array of frontend open orders:
     *         [
     *           {
     *             children: [...],           // Child orders (e.g., TP/SL)
     *             coin: str,                 // Asset name
     *             isPositionTpsl: bool,      // Is position take-profit/stop-loss
     *             isTrigger: bool,           // Is trigger order
     *             limitPx: float string,     // Limit price
     *             oid: int,                  // Order ID
     *             orderType: str,            // Order type (Limit, Stop Market, etc.)
     *             origSz: float string,      // Original size
     *             reduceOnly: bool,          // Reduce-only flag
     *             side: "A" | "B",           // Ask or Bid
     *             sz: float string,          // Current size
     *             tif: str,                  // Time-in-force (Gtc, Ioc, Alo)
     *             timestamp: int,            // Order timestamp (ms)
     *             triggerCondition: str,     // Trigger condition
     *             triggerPx: float string    // Trigger price
     *           },
     *           ...
     *         ]
     */
    nlohmann::json frontendOpenOrders(const std::string& address, const std::string& dex = "");

    /**
     * Get all mid prices
     */
    nlohmann::json allMids(const std::string& dex = "");

    /**
     * Get user fills (trades)
     */
    nlohmann::json userFills(const std::string& address);

    /**
     * Get user fills within time range
     */
    nlohmann::json userFillsByTime(const std::string& address,
                                   int64_t start_time,
                                   std::optional<int64_t> end_time = std::nullopt);

    /**
     * Get perpetuals metadata
     */
    Meta meta(const std::string& dex = "");

    /**
     * Get spot metadata
     */
    SpotMeta spotMeta();

    /**
     * Get L2 order book snapshot
     */
    nlohmann::json l2Snapshot(const std::string& name);

    /**
     * Query order by OID
     */
    nlohmann::json queryOrderByOid(const std::string& user, int64_t oid);

    /**
     * Query order by client order ID
     */
    nlohmann::json queryOrderByCloid(const std::string& user, const Cloid& cloid);

    /**
     * Retrieve OHLCV candle data for a coin
     *
     * Only the most recent 5000 candles are available.
     * Supported intervals: "1m", "3m", "5m", "15m", "30m", "1h", "2h", "4h",
     *                      "8h", "12h", "1d", "3d", "1w", "1M"
     *
     * @param name       Coin name (e.g. "ETH"). Resolved via nameToCoin mapping.
     * @param interval   Candle interval string (e.g. "1h")
     * @param start_time Start time in milliseconds (inclusive)
     * @param end_time   Optional end time in milliseconds (inclusive). Defaults to current time.
     * @return Array of candle records:
     *         [
     *           {
     *             T: int,          // Candle close time (ms)
     *             t: int,          // Candle open time (ms)
     *             s: str,          // Coin name
     *             i: str,          // Interval
     *             o: float string, // Open price
     *             h: float string, // High price
     *             l: float string, // Low price
     *             c: float string, // Close price
     *             v: float string, // Volume (base asset)
     *             n: int           // Number of trades
     *           },
     *           ...
     *         ]
     */
    nlohmann::json candlesSnapshot(const std::string& name,
                                   const std::string& interval,
                                   int64_t start_time,
                                   std::optional<int64_t> end_time = std::nullopt);

    /**
     * Retrieve a user's TWAP slice fills (last 2000)
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of TWAP slice fill records:
     *         [
     *           {
     *             fill: {
     *               closedPnl: float string,
     *               coin: str,
     *               crossed: bool,
     *               dir: str,           // e.g. "Open Long", "Sell"
     *               hash: str,          // Always 0x000...0 for TWAP fills
     *               oid: int,
     *               px: float string,
     *               side: "A" | "B",
     *               startPosition: float string,
     *               sz: float string,
     *               time: int,
     *               fee: float string,
     *               feeToken: str,
     *               tid: int
     *             },
     *             twapId: int           // TWAP order ID
     *           },
     *           ...
     *         ]
     */
    nlohmann::json userTwapSliceFills(const std::string& user);

    /**
     * Query a user's fee schedule, volume tier, and staking discounts
     *
     * @param user Address in 42-character hexadecimal format
     * @return Object containing:
     *         {
     *           dailyUserVlm: [{date, userCross, userAdd, exchange}, ...],
     *           feeSchedule: {
     *             cross: str, add: str, spotCross: str, spotAdd: str,
     *             tiers: { vip: [...], mm: [...] },
     *             referralDiscount: str,
     *             stakingDiscountTiers: [{bpsOfMaxSupply, discount}, ...]
     *           },
     *           userCrossRate: str,      // Effective cross fee rate
     *           userAddRate: str,        // Effective add (maker) fee rate
     *           userSpotCrossRate: str,
     *           userSpotAddRate: str,
     *           activeReferralDiscount: str,
     *           trial: null | {...},
     *           feeTrialReward: str,
     *           nextTrialAvailableTimestamp: null | int,
     *           stakingLink: {type, stakingUser} | null,
     *           activeStakingDiscount: {bpsOfMaxSupply, discount} | null
     *         }
     */
    nlohmann::json userFees(const std::string& user);

    /**
     * Check the maximum builder fee a user has approved for a builder
     *
     * @param user    Address in 42-character hexadecimal format
     * @param builder Builder address in 42-character hexadecimal format
     * @return Integer: maximum approved fee in tenths of a basis point
     *         (e.g. 1 means 0.001%, 10 means 0.01%)
     */
    nlohmann::json maxBuilderFee(const std::string& user, const std::string& builder);

    /**
     * Retrieve a user's historical orders (last 2000)
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of historical order records, each containing:
     *         {
     *           order: {
     *             coin: str,               // Asset name
     *             side: "A" | "B",         // Ask (sell) or Bid (buy)
     *             limitPx: float string,   // Limit price
     *             sz: float string,        // Remaining size
     *             oid: int,                // Order ID
     *             timestamp: int,          // Creation timestamp (ms)
     *             triggerCondition: str,   // e.g. "N/A", "tp", "sl"
     *             isTrigger: bool,
     *             triggerPx: float string,
     *             children: [...],         // Child TP/SL orders
     *             isPositionTpsl: bool,
     *             reduceOnly: bool,
     *             orderType: str,          // e.g. "Limit", "Market", "Stop Market"
     *             origSz: float string,    // Original size at placement
     *             tif: str,               // Time-in-force: "Gtc", "Ioc", "Alo", "FrontendMarket"
     *             cloid: str | null        // Client order ID if set
     *           },
     *           status: str,               // "filled", "open", "canceled", "triggered",
     *                                      // "rejected", "marginCanceled", etc.
     *           statusTimestamp: int       // Timestamp of last status change (ms)
     *         }
     */
    nlohmann::json historicalOrders(const std::string& user);

    /**
     * Retrieve historical funding rates for a coin
     *
     * @param name Coin name (e.g. "ETH"). Resolved via nameToCoin mapping.
     * @param start_time Start time in milliseconds (inclusive)
     * @param end_time Optional end time in milliseconds (inclusive). Defaults to current time.
     * @return Array of funding rate records:
     *         [
     *           {
     *             coin: str,           // Coin name
     *             fundingRate: str,    // Funding rate (float string)
     *             premium: str,        // Premium (float string)
     *             time: int            // Timestamp in milliseconds
     *           },
     *           ...
     *         ]
     */
    nlohmann::json fundingHistory(const std::string& name,
                                  int64_t start_time,
                                  std::optional<int64_t> end_time = std::nullopt);

    /**
     * Retrieve a user's funding payment history
     *
     * @param user Address in 42-character hexadecimal format
     * @param start_time Start time in milliseconds (inclusive)
     * @param end_time Optional end time in milliseconds (inclusive). Defaults to current time.
     * @return Array of funding payment records:
     *         [
     *           {
     *             delta: {
     *               coin: str,          // Coin name
     *               fundingRate: str,   // Applied funding rate
     *               szi: str,           // Position size at time of funding
     *               type: "funding",
     *               usdc: str,          // USDC amount paid/received (negative = paid)
     *               nSamples: int|null
     *             },
     *             hash: str,            // Transaction hash
     *             time: int             // Timestamp in milliseconds
     *           },
     *           ...
     *         ]
     */
    nlohmann::json userFundingHistory(const std::string& user,
                                      int64_t start_time,
                                      std::optional<int64_t> end_time = std::nullopt);

    /**
     * Query sub-accounts for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of sub-account objects, each containing clearinghouseState and spotState,
     *         or null if user has no sub-accounts.
     *         [
     *           {
     *             subAccountUser: str,       // Sub-account address
     *             name: str,                 // Sub-account name
     *             master: str,               // Master account address
     *             clearinghouseState: {...},  // Perp positions, margin summary
     *             spotState: {...}            // Spot balances
     *           },
     *           ...
     *         ]
     */
    nlohmann::json querySubAccounts(const std::string& user);

    /**
     * Query referral information for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Object containing referral code, cumulative VLM, unclaimed/claimed rewards,
     *         and referred user history:
     *         {
     *           referredBy: { referrer: str, code: str } | null,
     *           cumVlm: str,
     *           unclaimedRewards: str,
     *           claimedRewards: str,
     *           builderRewards: str,
     *           referrerState: {
     *             stage: str,
     *             data: {
     *               code: str,
     *               referralStates: [
     *                 { cumVlm: str, cumRewardedFeesSinceReferred: str, ... },
     *                 ...
     *               ]
     *             }
     *           },
     *           rewardHistory: [...]
     *         }
     */
    nlohmann::json queryReferralState(const std::string& user);

    /**
     * Get list of approved builder addresses for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of approved builder addresses:
     *         ["0x476fa87b4d3818f437f38f1263bee508d7672d82", ...]
     */
    nlohmann::json approvedBuilders(const std::string& user);

    /**
     * Query role and account type for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Object with role field: "missing", "user", "agent", "vault", or "subAccount"
     *         For agents: {"role": "agent", "data": {"user": "0x..."}}
     *         For subaccounts: {"role": "subAccount", "data": {"master": "0x..."}}
     *         For others: {"role": "<type>"}
     */
    nlohmann::json userRole(const std::string& user);

    /**
     * Query API rate limit configuration and usage for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Object containing:
     *         {
     *           cumVlm: str,            // Cumulative volume
     *           nRequestsUsed: int,     // max(0, cumulative_used minus reserved)
     *           nRequestsCap: int,      // Request capacity
     *           nRequestsSurplus: int   // max(0, reserved minus cumulative_used)
     *         }
     */
    nlohmann::json userRateLimit(const std::string& user);

    /**
     * Retrieve comprehensive portfolio performance data
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of [period, data] pairs where period is one of:
     *         "day", "week", "month", "allTime", "perpDay", "perpWeek",
     *         "perpMonth", "perpAllTime"
     *         Each data object contains:
     *         {
     *           accountValueHistory: [[timestamp, value_str], ...],
     *           pnlHistory: [[timestamp, pnl_str], ...],
     *           vlm: str
     *         }
     */
    nlohmann::json portfolio(const std::string& user);

    /**
     * Retrieve non-funding ledger updates (deposits, withdrawals, transfers,
     * liquidations, and other account activities excluding funding payments)
     *
     * @param user Address in 42-character hexadecimal format
     * @param start_time Start time in milliseconds (epoch timestamp)
     * @param end_time Optional end time in milliseconds (epoch timestamp)
     * @return Array of ledger update records
     */
    nlohmann::json userNonFundingLedgerUpdates(const std::string& user,
                                                int64_t start_time,
                                                std::optional<int64_t> end_time = std::nullopt);

    /**
     * Get vault details including portfolio history, followers, APR, commission
     *
     * @param vault_address Vault address in 42-character hexadecimal format
     * @param user Optional user address to get follower-specific state
     * @return Object containing:
     *         {
     *           name: str,
     *           vaultAddress: str,
     *           leader: str,
     *           description: str,
     *           portfolio: [[period, {accountValueHistory, pnlHistory, vlm}], ...],
     *           apr: float,
     *           followerState: null | {...},
     *           leaderFraction: float,
     *           leaderCommission: float,
     *           followers: [{user, vaultEquity, pnl, allTimePnl, daysFollowing, ...}, ...],
     *           maxDistributable: float,
     *           maxWithdrawable: float,
     *           isClosed: bool,
     *           relationship: {type, data} | null,
     *           allowDeposits: bool,
     *           alwaysCloseOnWithdraw: bool
     *         }
     */
    nlohmann::json vaultDetails(const std::string& vault_address,
                                 const std::string& user = "");

    /**
     * Retrieve user's equity positions across all vaults
     *
     * @param user Address in 42-character hexadecimal format
     * @return Array of vault equity objects:
     *         [
     *           {
     *             vaultAddress: str,
     *             equity: str
     *           },
     *           ...
     *         ]
     */
    nlohmann::json userVaultEquities(const std::string& user);

    /**
     * Query HIP-3 DEX abstraction state for a user
     *
     * @param user Address in 42-character hexadecimal format
     * @return Boolean: true if DEX abstraction is enabled, false otherwise
     */
    nlohmann::json queryUserDexAbstractionState(const std::string& user);

    /**
     * Query user abstraction mode
     *
     * @param user Address in 42-character hexadecimal format
     * @return String: one of "unifiedAccount", "portfolioMargin", "disabled",
     *         "default", or "dexAbstraction"
     */
    nlohmann::json queryUserAbstractionState(const std::string& user);

    /**
     * Retrieve perpetuals metadata and asset contexts (mark price, funding, OI, etc.)
     *
     * @param dex Perp dex name. Defaults to empty string (first perp dex).
     * @return 2-element array:
     *         [0]: Meta object {universe, marginTables, collateralToken}
     *         [1]: Array of asset contexts, one per universe entry:
     *              {dayNtlVlm, funding, impactPxs, markPx, midPx,
     *               openInterest, oraclePx, premium, prevDayPx}
     */
    nlohmann::json metaAndAssetCtxs(const std::string& dex = "");

    /**
     * Retrieve spot metadata and asset contexts
     *
     * @return 2-element array:
     *         [0]: SpotMeta {tokens, universe}
     *         [1]: Array of spot asset contexts:
     *              {dayNtlVlm, markPx, midPx, prevDayPx, circulatingSupply, coin}
     */
    nlohmann::json spotMetaAndAssetCtxs();

    /**
     * Retrieve all perpetual dexes
     *
     * @return Array where [0] is null (default dex) and subsequent entries are:
     *         {name, fullName, deployer, oracleUpdater, feeRecipient,
     *          assetToStreamingOiCap, assetToFundingMultiplier}
     */
    nlohmann::json perpDexs();

    /**
     * Retrieve perp deploy auction status
     *
     * @return Object: {startTimeSeconds, durationSeconds, startGas, currentGas, endGas}
     */
    nlohmann::json queryPerpDeployAuctionStatus();

    /**
     * Manually register perpetual metadata
     * Users must call this to enable nameToAsset() for perp markets
     */
    void registerPerpMeta(const Meta& meta, int offset = 0);

    /**
     * Manually register spot metadata
     * Users must call this to enable nameToAsset() for spot markets
     */
    void registerSpotMeta(const SpotMeta& spot_meta);

    // Metadata caches (public for Exchange class access)
    std::unordered_map<std::string, int> coin_to_asset_;
    std::unordered_map<std::string, std::string> name_to_coin_;
    std::unordered_map<int, int> asset_to_sz_decimals_;

private:
    void initializeMetadata(const Meta* meta,
                           const SpotMeta* spot_meta,
                           const std::vector<std::string>* perp_dexs);

    void setPerpMeta(const Meta& meta, int offset);
};

} // namespace hyperliquid
