#pragma once

#include "hyperliquid/api.hpp"
#include "hyperliquid/info.hpp"
#include "hyperliquid/types.hpp"
#include "hyperliquid/utils/signing.hpp"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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
     * Place a TWAP order.
     *
     * The order is split into suborders executed in 30 second intervals over
     * `minutes`. Each suborder has a maximum slippage of 3%.
     *
     * The size is rounded to the asset's szDecimals, same as order().
     *
     * On success the response carries the twap id, which is what twapCancel()
     * takes:
     *   response["response"]["data"]["status"]["running"]["twapId"]
     *
     * Note that a rejection is still HTTP 200 with status "ok"; the failure
     * surfaces as response["response"]["data"]["status"]["error"], e.g.
     * "Invalid TWAP duration: 1 min(s)". Check it before assuming success.
     *
     * @param coin Coin name (e.g. "ETH")
     * @param is_buy True to buy, false to sell
     * @param sz Total size to execute across the whole TWAP
     * @param minutes Duration to spread the order over. Valid range is enforced
     *                by the API, not here.
     * @param reduce_only Only reduce an existing position
     * @param randomize Randomize suborder timing
     */
    nlohmann::json twapOrder(const std::string& coin,
                            bool is_buy,
                            double sz,
                            int minutes,
                            bool reduce_only = false,
                            bool randomize = false);

    /**
     * Cancel a running TWAP order.
     * Failure surfaces like twapOrder(); see above.
     *
     * @param coin Coin name (e.g. "ETH"). Must match the coin the TWAP was placed on.
     * @param twap_id The twap id returned by twapOrder()
     */
    nlohmann::json twapCancel(const std::string& coin, int64_t twap_id);

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
     * Transfer a token between dexes (e.g. spot <-> a perp dex, or between
     * perp dexes) for the same account.
     *
     * Dex names: "" is the default perp dex, "spot" is spot. When transferring
     * to or from a perp dex, the token must be that dex's collateral token.
     * Token uses the same "NAME:0x<token-id>" form as spotTransfer.
     *
     * User-signed action (EIP-712) carrying its own nonce and no vaultAddress
     * field. When the Exchange was constructed with a vault/subaccount
     * address, it is sent as the signed fromSubAccount field, matching the
     * Python SDK.
     *
     * @param destination Recipient address (42-char hex)
     * @param source_dex Dex to send from ("" = default perp dex, "spot" = spot)
     * @param destination_dex Dex to send to
     * @param token Token name, e.g. "USDC:0x..." or plain collateral token name
     * @param amount Amount of the token to send
     */
    nlohmann::json sendAsset(const std::string& destination,
                            const std::string& source_dex,
                            const std::string& destination_dex,
                            const std::string& token,
                            double amount);

    /**
     * Transfer USDC between the spot and perp balances of the same account.
     *
     * User-signed action (EIP-712), so it carries its own nonce and no
     * vaultAddress field. When the Exchange was constructed with a vault or
     * subaccount address, the transfer applies to that subaccount: the wire
     * amount becomes "<amount> subaccount:<address>", matching the Python SDK.
     *
     * @param amount USDC amount to move
     * @param to_perp true: spot -> perp, false: perp -> spot
     */
    nlohmann::json usdClassTransfer(double amount, bool to_perp);

    /**
     * Approve an agent (API) wallet that can sign L1 actions (orders, cancels,
     * ...) on behalf of this account, without being able to transfer funds.
     *
     * A fresh agent private key is generated locally, its address is approved,
     * and the key is returned alongside the API response. Persist the key —
     * it is shown nowhere else. Use it later via
     * Wallet::fromPrivateKey(agent_key).
     *
     * Python SDK parity note: the action is always signed with an agentName
     * field ("" when unnamed), but the field is omitted from the wire when no
     * name was given. An unnamed agent replaces the previous unnamed agent;
     * named agents coexist (the API caps how many).
     *
     * User-signed action (EIP-712, HyperliquidTransaction:ApproveAgent), so it
     * must be signed by the account's own wallet, not another agent wallet.
     *
     * @param name Optional agent name. nullopt approves an unnamed agent.
     * @return {API response, generated agent private key ("0x" + 64 hex)}
     */
    std::pair<nlohmann::json, std::string> approveAgent(
        const std::optional<std::string>& name = std::nullopt);

    /**
     * Approve a maximum builder fee rate for a builder address, allowing that
     * builder to attach fees up to the rate on orders placed through it
     * (the BuilderInfo parameter of order()/bulkOrders()).
     *
     * Check the currently approved rate with info_.maxBuilderFee(user, builder).
     *
     * User-signed action (EIP-712, HyperliquidTransaction:ApproveBuilderFee),
     * so it must be signed by the account's own wallet, not an agent wallet.
     *
     * @param builder Builder address (42-char hex)
     * @param max_fee_rate Maximum fee rate as a percent string, e.g. "0.001%"
     */
    nlohmann::json approveBuilderFee(const std::string& builder,
                                    const std::string& max_fee_rate);

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
     * Update isolated margin for a position.
     * @param amount Amount in USD to add (positive) or remove (negative)
     * @param coin Coin name (e.g. "ETH")
     */
    nlohmann::json updateIsolatedMargin(double amount, const std::string& coin);

    /**
     * Top up the margin of an isolated position so that its effective leverage
     * drops to the target value.
     *
     * Distinct from updateIsolatedMargin(): that one moves a fixed USDC amount
     * in or out, while this one lets the exchange compute how much margin to
     * add to reach `leverage`. Top-up only — it never removes margin.
     *
     * Wire action: {"type": "topUpIsolatedOnlyMargin", "asset": N, "leverage": "<float string>"}
     *
     * @param coin Coin name of the isolated position (e.g. "ETH")
     * @param leverage Target leverage, e.g. 5.0 for 5x. Must be positive.
     */
    nlohmann::json topUpIsolatedOnlyMargin(const std::string& coin, double leverage);

    /**
     * Buy extra request weight to raise this address's rate limit instead of
     * being throttled. Pairs with the info_.userRateLimit() query, which reports
     * cumulative volume, requests used, and the current cap.
     *
     * Costs are charged by the exchange per unit of weight; this SDK does not
     * model the price. A successful reservation raises nRequestsCap.
     *
     * Wire action: {"type": "reserveRequestWeight", "weight": N}
     *
     * @param weight Number of request-weight units to reserve. Must be positive.
     */
    nlohmann::json reserveRequestWeight(int64_t weight);

    /**
     * Set expiration time for actions (optional)
     */
    void setExpiresAfter(std::optional<int64_t> expires_after);

    // Public info object for queries
    Info info_;

private:
    /**
     * Sign an L1 action with the current nonce, vault address and expiry, then
     * post it. Every L1 action (orders, cancels, modifies, leverage, twap, ...)
     * goes through here so the signing inputs stay in one place.
     *
     * Not used by the user-signed actions (usdTransfer, spotTransfer), which
     * sign an EIP-712 payload instead and carry their own nonce.
     */
    nlohmann::json postL1Action(const nlohmann::ordered_json& action);

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
