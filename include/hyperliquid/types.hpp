#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <nlohmann/json.hpp>

namespace hyperliquid {

// Forward declarations
class Cloid;

/**
 * Signature structure (r, s, v components)
 */
struct Signature {
    std::string r;  // hex string with "0x" prefix
    std::string s;  // hex string with "0x" prefix
    int v;          // recovery id (27 or 28)

    nlohmann::json toJson() const {
        return {{"r", r}, {"s", s}, {"v", v}};
    }
};

/**
 * Client Order ID - 16-byte hex string with "0x" prefix
 */
class Cloid {
public:
    explicit Cloid(const std::string& raw);
    static Cloid fromInt(uint64_t value);
    static Cloid fromStr(const std::string& hex);

    std::string toRaw() const { return raw_cloid_; }

private:
    void validate();
    std::string raw_cloid_;
};

/**
 * Time in Force for limit orders
 */
struct LimitOrderType {
    std::string tif;  // "Alo", "Ioc", or "Gtc"

    nlohmann::json toJson() const {
        return {{"tif", tif}};
    }
};

/**
 * Trigger order configuration
 */
struct TriggerOrderType {
    double trigger_px;
    bool is_market;
    std::string tpsl;  // "tp" (take profit) or "sl" (stop loss)

    nlohmann::json toJson() const {
        return {
            {"triggerPx", trigger_px},
            {"isMarket", is_market},
            {"tpsl", tpsl}
        };
    }
};

/**
 * Order type specification (limit or trigger)
 */
struct OrderType {
    std::optional<LimitOrderType> limit;
    std::optional<TriggerOrderType> trigger;

    nlohmann::json toJson() const;
};

/**
 * Order request structure
 */
struct OrderRequest {
    std::string coin;
    bool is_buy;
    double sz;
    double limit_px;
    OrderType order_type;
    bool reduce_only;
    std::optional<Cloid> cloid;
};

/**
 * Order wire format (for API transmission)
 */
struct OrderWire {
    int asset;                      // "a"
    bool is_buy;                    // "b"
    std::string price;              // "p" - 8 decimal string
    std::string size;               // "s" - 8 decimal string
    bool reduce_only;               // "r"
    nlohmann::json order_type;      // "t"
    std::optional<std::string> cloid;  // "c"

    nlohmann::ordered_json toJson() const;
};

/**
 * Cancel request
 */
struct CancelRequest {
    std::string coin;
    int64_t oid;
};

/**
 * Cancel by client order ID request
 */
struct CancelByCloidRequest {
    std::string coin;
    Cloid cloid;
};

/**
 * OID or CLOID variant for modify operations
 */
using OidOrCloid = std::variant<int64_t, Cloid>;

/**
 * Modify order request
 */
struct ModifyRequest {
    OidOrCloid oid;
    OrderRequest order;
};

/**
 * TWAP order wire format (for API transmission)
 * A TWAP splits a large order into suborders executed in 30s intervals,
 * each with a maximum slippage of 3%.
 */
struct TwapWire {
    int asset;          // "a"
    bool is_buy;        // "b"
    std::string size;   // "s" - 8 decimal string
    bool reduce_only;   // "r"
    int minutes;        // "m" - duration over which the order is spread
    bool randomize;     // "t" - randomize suborder timing

    nlohmann::ordered_json toJson() const;
};

/**
 * One rung of a margin table: above `lower_bound` notional, an asset's
 * leverage is capped at `max_leverage`. Tiers are listed ascending, and the
 * first always has lower_bound "0.0".
 */
struct MarginTier {
    std::string lower_bound;  // decimal string, e.g. "3000000.0"
    int max_leverage = 0;
};

/**
 * A leverage schedule shared by assets that reference it via
 * AssetInfo::margin_table_id. A single-tier table is a flat leverage cap.
 */
struct MarginTable {
    int id = 0;
    std::string description;  // e.g. "tiered 10x"; often empty
    std::vector<MarginTier> margin_tiers;
};

/**
 * Asset information.
 *
 * Only name and sz_decimals are always present; the rest are absent on some
 * dexes, so they carry defaults (or nullopt) rather than throwing. The
 * growth_mode fields appear only on builder-deployed (HIP-3) dexes.
 */
struct AssetInfo {
    std::string name;
    int sz_decimals = 0;
    int max_leverage = 0;
    // Indexes into Meta::margin_tables. nullopt means the dex did not report
    // one, in which case max_leverage is the whole story.
    std::optional<int> margin_table_id;
    // "strictIsolated" (margin cannot be withdrawn from an open position) or
    // "noCross" (isolated margin only).
    std::optional<std::string> margin_mode;
    std::optional<std::string> growth_mode;                     // e.g. "enabled"
    std::optional<std::string> last_growth_mode_change_time;    // ISO-8601
    // Deprecated by the API in favour of margin_mode, which distinguishes the
    // two cases this bool conflates. Still sent, so still parsed.
    bool only_isolated = false;
    bool is_delisted = false;
};

/**
 * Perpetuals metadata
 */
struct Meta {
    std::vector<AssetInfo> universe;
    std::vector<MarginTable> margin_tables;
    // Token index of the dex's collateral (0 = USDC on the default dex).
    int collateral_token = 0;
};

/**
 * Spot asset information
 */
struct SpotAssetInfo {
    std::string name;
    std::vector<int> tokens;
    int index;
    bool is_canonical;
};

/**
 * Spot token information
 */
struct SpotTokenInfo {
    std::string name;
    int sz_decimals;
    int wei_decimals;
    int index;
    std::string token_id;
    bool is_canonical;
};

/**
 * Spot metadata
 */
struct SpotMeta {
    std::vector<SpotAssetInfo> universe;
    std::vector<SpotTokenInfo> tokens;
};

/**
 * Builder fee information
 */
struct BuilderInfo {
    std::string b;  // builder address (lowercase)
    int f;          // fee in tenths of basis points
};

/**
 * EIP-712 type definition
 */
struct EIP712Type {
    std::string name;
    std::string type;

    nlohmann::json toJson() const {
        return {{"name", name}, {"type", type}};
    }
};

} // namespace hyperliquid
