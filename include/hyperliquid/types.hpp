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

    nlohmann::json toJson() const;
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
 * Asset information
 */
struct AssetInfo {
    std::string name;
    int sz_decimals;
};

/**
 * Perpetuals metadata
 */
struct Meta {
    std::vector<AssetInfo> universe;
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
