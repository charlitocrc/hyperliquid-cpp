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
