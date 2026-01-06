#pragma once

#include "hyperliquid/api.hpp"
#include "hyperliquid/info.hpp"
#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/signing.hpp"
#include <memory>
#include <vector>
#include <optional>

namespace hyperliquid {

/**
 * Exchange class for trading operations
 */
class Exchange : public API {
public:
    static constexpr double DEFAULT_SLIPPAGE = 0.05;

    explicit Exchange(std::shared_ptr<Wallet> wallet,
                     const std::string& base_url = "",
                     const Meta* meta = nullptr,
                     const std::string& vault_address = "",
                     const std::string& account_address = "",
                     const SpotMeta* spot_meta = nullptr,
                     const std::vector<std::string>* perp_dexs = nullptr,
                     int timeout_ms = 30000);

    /**
     * Place a single order
     */
    nlohmann::json order(const std::string& coin,
                        bool is_buy,
                        double sz,
                        double limit_px,
                        const OrderType& order_type,
                        bool reduce_only = false,
                        const std::optional<Cloid>& cloid = std::nullopt,
                        const std::optional<BuilderInfo>& builder = std::nullopt);

    /**
     * Place multiple orders in a single request
     */
    nlohmann::json bulkOrders(const std::vector<OrderRequest>& orders,
                             const std::optional<BuilderInfo>& builder = std::nullopt,
                             const std::string& grouping = "na");

    /**
     * Open a market order
     */
    nlohmann::json marketOpen(const std::string& coin,
                             bool is_buy,
                             double sz,
                             std::optional<double> px = std::nullopt,
                             double slippage = DEFAULT_SLIPPAGE,
                             const std::optional<Cloid>& cloid = std::nullopt,
                             const std::optional<BuilderInfo>& builder = std::nullopt);

    /**
     * Close a position with market order
     */
    nlohmann::json marketClose(const std::string& coin,
                              std::optional<double> sz = std::nullopt,
                              std::optional<double> px = std::nullopt,
                              double slippage = DEFAULT_SLIPPAGE,
                              const std::optional<Cloid>& cloid = std::nullopt,
                              const std::optional<BuilderInfo>& builder = std::nullopt);

    /**
     * Cancel an order by OID
     */
    nlohmann::json cancel(const std::string& coin, int64_t oid);

    /**
     * Cancel an order by client order ID
     */
    nlohmann::json cancelByCloid(const std::string& coin, const Cloid& cloid);

    /**
     * Cancel multiple orders
     */
    nlohmann::json bulkCancel(const std::vector<CancelRequest>& cancels);

    /**
     * Cancel multiple orders by CLOID
     */
    nlohmann::json bulkCancelByCloid(const std::vector<CancelByCloidRequest>& cancels);

    /**
     * Modify an existing order
     */
    nlohmann::json modifyOrder(const OidOrCloid& oid,
                              const std::string& coin,
                              bool is_buy,
                              double sz,
                              double limit_px,
                              const OrderType& order_type,
                              bool reduce_only = false,
                              const std::optional<Cloid>& cloid = std::nullopt);

    /**
     * Modify multiple orders
     */
    nlohmann::json bulkModifyOrders(const std::vector<ModifyRequest>& modifies);

    /**
     * Transfer USD to another address
     */
    nlohmann::json usdTransfer(double amount, const std::string& destination);

    /**
     * Transfer spot tokens to another address
     */
    nlohmann::json spotTransfer(double amount,
                               const std::string& destination,
                               const std::string& token);

    /**
     * Update leverage for a coin
     */
    nlohmann::json updateLeverage(int leverage,
                                 const std::string& coin,
                                 bool is_cross = true);

    /**
     * Schedule future cancel of all open orders.
     * The time must be at least 5 seconds after the current time.
     * Once the time comes, all open orders will be canceled and a trigger count
     * will be incremented. The max number of triggers per day is 10.
     * This trigger count is reset at 00:00 UTC.
     *
     * @param time If provided, set the cancel time (UTC millis). If nullopt, unsets any scheduled cancel.
     */
    nlohmann::json scheduleCancel(std::optional<int64_t> time = std::nullopt);

    /**
     * Query order status by client order ID.
     * Convenience method that delegates to info_.queryOrderByCloid().
     */
    nlohmann::json queryOrderByCloid(const std::string& user, const Cloid& cloid);

    /**
     * Set expiration time for actions (optional)
     */
    void setExpiresAfter(std::optional<int64_t> expires_after);

    // Public info object for queries
    Info info_;

private:
    nlohmann::json postAction(const nlohmann::json& action,
                             const Signature& signature,
                             int64_t nonce);

    double slippagePrice(const std::string& name,
                        bool is_buy,
                        double slippage,
                        std::optional<double> px = std::nullopt);

    std::shared_ptr<Wallet> wallet_;
    std::string vault_address_;
    std::string account_address_;
    std::optional<int64_t> expires_after_;
};

} // namespace hyperliquid
