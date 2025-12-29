#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/constants.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <cmath>

namespace hyperliquid {

Exchange::Exchange(std::shared_ptr<Wallet> wallet,
                  const std::string& base_url,
                  const Meta* meta,
                  const std::string& vault_address,
                  const std::string& account_address,
                  const SpotMeta* spot_meta,
                  const std::vector<std::string>* perp_dexs,
                  int timeout_ms)
    : API(base_url.empty() ? MAINNET_API_URL : base_url, timeout_ms),
      info_(base_url, true, meta, spot_meta, perp_dexs, timeout_ms),
      wallet_(wallet),
      vault_address_(vault_address),
      account_address_(account_address),
      expires_after_(std::nullopt) {
}

nlohmann::json Exchange::postAction(const nlohmann::json& action,
                                    const Signature& signature,
                                    int64_t nonce) {
    nlohmann::json payload = {
        {"action", action},
        {"nonce", nonce},
        {"signature", signature.toJson()}
    };

    // Add vault address if not a transfer action
    std::string action_type = action["type"];
    if (action_type != "usdClassTransfer" && action_type != "sendAsset") {
        if (!vault_address_.empty()) {
            payload["vaultAddress"] = vault_address_;
        } else {
            payload["vaultAddress"] = nullptr;
        }
    }

    // Add expires after if set
    if (expires_after_.has_value()) {
        payload["expiresAfter"] = expires_after_.value();
    } else {
        payload["expiresAfter"] = nullptr;
    }

    return post("/exchange", payload);
}

double Exchange::slippagePrice(const std::string& name,
                              bool is_buy,
                              double slippage,
                              std::optional<double> px) {
    std::string coin = info_.nameToCoin(name);

    // Get mid price if not provided
    if (!px.has_value()) {
        auto mids = info_.allMids("");
        px = std::stod(mids[coin].get<std::string>());
    }

    int asset = info_.coin_to_asset_[coin];
    bool is_spot = asset >= 10000;
    int sz_decimals = info_.asset_to_sz_decimals_[asset];

    // Calculate slippage
    double price = px.value();
    price *= is_buy ? (1.0 + slippage) : (1.0 - slippage);

    // Round to tick size (5 significant figures and MAX_DECIMALS - szDecimals)
    return roundPrice(price, sz_decimals, is_spot);
}

void Exchange::setExpiresAfter(std::optional<int64_t> expires_after) {
    expires_after_ = expires_after;
}

nlohmann::json Exchange::order(const std::string& coin,
                               bool is_buy,
                               double sz,
                               double limit_px,
                               const OrderType& order_type,
                               bool reduce_only,
                               const std::optional<Cloid>& cloid,
                               const std::optional<BuilderInfo>& builder) {
    // Get asset info for rounding
    int asset = info_.nameToAsset(coin);
    int sz_decimals = info_.asset_to_sz_decimals_[asset];
    bool is_spot = asset >= 10000;

    // Round price and size to tick/lot size
    double rounded_px = roundPrice(limit_px, sz_decimals, is_spot);
    double rounded_sz = roundSize(sz, sz_decimals);

    OrderRequest order_req;
    order_req.coin = coin;
    order_req.is_buy = is_buy;
    order_req.sz = rounded_sz;
    order_req.limit_px = rounded_px;
    order_req.order_type = order_type;
    order_req.reduce_only = reduce_only;
    order_req.cloid = cloid;

    return bulkOrders({order_req}, builder);
}

nlohmann::json Exchange::bulkOrders(const std::vector<OrderRequest>& orders,
                                    const std::optional<BuilderInfo>& builder,
                                    const std::string& grouping) {
    std::vector<OrderWire> order_wires;
    for (const auto& order : orders) {
        int asset = info_.nameToAsset(order.coin);
        int sz_decimals = info_.asset_to_sz_decimals_[asset];
        bool is_spot = asset >= 10000;

        // Round price and size to tick/lot size
        OrderRequest rounded_order = order;
        rounded_order.limit_px = roundPrice(order.limit_px, sz_decimals, is_spot);
        rounded_order.sz = roundSize(order.sz, sz_decimals);

        order_wires.push_back(orderRequestToOrderWire(rounded_order, asset));
    }

    int64_t timestamp = getTimestampMs();

    // Create order action
    auto action = orderWiresToOrderAction(order_wires, builder, grouping);

    // Determine if mainnet
    bool is_mainnet = (base_url_ == MAINNET_API_URL);

    // Sign action
    std::optional<std::string> vault_opt = vault_address_.empty() ?
        std::nullopt : std::optional<std::string>(vault_address_);
    auto signature = signL1Action(*wallet_, action, vault_opt, timestamp,
                                 expires_after_, is_mainnet);

    return postAction(action, signature, timestamp);
}

nlohmann::json Exchange::marketOpen(const std::string& coin,
                                    bool is_buy,
                                    double sz,
                                    std::optional<double> px,
                                    double slippage,
                                    const std::optional<Cloid>& cloid,
                                    const std::optional<BuilderInfo>& builder) {
    double price = slippagePrice(coin, is_buy, slippage, px);

    OrderType order_type;
    order_type.limit = LimitOrderType{"Ioc"};  // Immediate or cancel

    return order(coin, is_buy, sz, price, order_type, false, cloid, builder);
}

nlohmann::json Exchange::marketClose(const std::string& coin,
                                     std::optional<double> sz,
                                     std::optional<double> px,
                                     double slippage,
                                     const std::optional<Cloid>& cloid,
                                     const std::optional<BuilderInfo>& builder) {
    // Get user state to determine position size and direction
    std::string address = wallet_->address();
    auto user_state = info_.userState(address);

    // Find position
    double position_sz = 0.0;
    bool found = false;
    for (const auto& asset_pos : user_state["assetPositions"]) {
        auto pos = asset_pos["position"];
        if (pos["coin"] == coin) {
            position_sz = std::stod(pos["szi"].get<std::string>());
            found = true;
            break;
        }
    }

    if (!found || std::abs(position_sz) < 1e-8) {
        throw std::runtime_error("No position to close for " + coin);
    }

    // Determine close size and direction
    double close_sz = sz.has_value() ? sz.value() : std::abs(position_sz);
    bool is_buy = position_sz < 0;  // Buy to close short, sell to close long

    return marketOpen(coin, is_buy, close_sz, px, slippage, cloid, builder);
}

nlohmann::json Exchange::cancel(const std::string& coin, int64_t oid) {
    CancelRequest cancel_req;
    cancel_req.coin = coin;
    cancel_req.oid = oid;
    return bulkCancel({cancel_req});
}

nlohmann::json Exchange::cancelByCloid(const std::string& coin, const Cloid& cloid) {
    CancelByCloidRequest cancel_req{coin, cloid};
    return bulkCancelByCloid({cancel_req});
}

nlohmann::json Exchange::bulkCancel(const std::vector<CancelRequest>& cancels) {
    nlohmann::ordered_json cancels_array = nlohmann::ordered_json::array();
    for (const auto& cancel : cancels) {
        int asset = info_.nameToAsset(cancel.coin);
        nlohmann::ordered_json cancel_obj;
        cancel_obj["a"] = asset;
        cancel_obj["o"] = cancel.oid;
        cancels_array.push_back(cancel_obj);
    }

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = cancels_array;

    int64_t timestamp = getTimestampMs();
    bool is_mainnet = (base_url_ == MAINNET_API_URL);

    std::optional<std::string> vault_opt = vault_address_.empty() ?
        std::nullopt : std::optional<std::string>(vault_address_);
    auto signature = signL1Action(*wallet_, action, vault_opt, timestamp,
                                 expires_after_, is_mainnet);

    return postAction(action, signature, timestamp);
}

nlohmann::json Exchange::bulkCancelByCloid(const std::vector<CancelByCloidRequest>& cancels) {
    nlohmann::ordered_json cancels_array = nlohmann::ordered_json::array();
    for (const auto& cancel : cancels) {
        int asset = info_.nameToAsset(cancel.coin);
        nlohmann::ordered_json cancel_obj;
        cancel_obj["a"] = asset;
        cancel_obj["o"] = cancel.cloid.toRaw();
        cancels_array.push_back(cancel_obj);
    }

    nlohmann::ordered_json action;
    action["type"] = "cancel";
    action["cancels"] = cancels_array;

    int64_t timestamp = getTimestampMs();
    bool is_mainnet = (base_url_ == MAINNET_API_URL);

    std::optional<std::string> vault_opt = vault_address_.empty() ?
        std::nullopt : std::optional<std::string>(vault_address_);
    auto signature = signL1Action(*wallet_, action, vault_opt, timestamp,
                                 expires_after_, is_mainnet);

    return postAction(action, signature, timestamp);
}

nlohmann::json Exchange::modifyOrder(const OidOrCloid& oid,
                                     const std::string& coin,
                                     bool is_buy,
                                     double sz,
                                     double limit_px,
                                     const OrderType& order_type,
                                     bool reduce_only,
                                     const std::optional<Cloid>& cloid) {
    // Get asset info for rounding
    int asset = info_.nameToAsset(coin);
    int sz_decimals = info_.asset_to_sz_decimals_[asset];
    bool is_spot = asset >= 10000;

    // Round price and size to tick/lot size
    double rounded_px = roundPrice(limit_px, sz_decimals, is_spot);
    double rounded_sz = roundSize(sz, sz_decimals);

    ModifyRequest modify_req;
    modify_req.oid = oid;
    modify_req.order.coin = coin;
    modify_req.order.is_buy = is_buy;
    modify_req.order.sz = rounded_sz;
    modify_req.order.limit_px = rounded_px;
    modify_req.order.order_type = order_type;
    modify_req.order.reduce_only = reduce_only;
    modify_req.order.cloid = cloid;

    return bulkModifyOrders({modify_req});
}

nlohmann::json Exchange::bulkModifyOrders(const std::vector<ModifyRequest>& modifies) {
    nlohmann::ordered_json modifies_array = nlohmann::ordered_json::array();
    for (const auto& modify : modifies) {
        int asset = info_.nameToAsset(modify.order.coin);
        int sz_decimals = info_.asset_to_sz_decimals_[asset];
        bool is_spot = asset >= 10000;

        // Round price and size to tick/lot size
        OrderRequest rounded_order = modify.order;
        rounded_order.limit_px = roundPrice(modify.order.limit_px, sz_decimals, is_spot);
        rounded_order.sz = roundSize(modify.order.sz, sz_decimals);

        OrderWire wire = orderRequestToOrderWire(rounded_order, asset);

        nlohmann::ordered_json modify_wire;
        if (std::holds_alternative<int64_t>(modify.oid)) {
            modify_wire["oid"] = std::get<int64_t>(modify.oid);
        } else {
            modify_wire["oid"] = std::get<Cloid>(modify.oid).toRaw();
        }
        modify_wire["order"] = wire.toJson();

        modifies_array.push_back(modify_wire);
    }

    nlohmann::ordered_json action;
    action["type"] = "batchModify";
    action["modifies"] = modifies_array;

    int64_t timestamp = getTimestampMs();
    bool is_mainnet = (base_url_ == MAINNET_API_URL);

    std::optional<std::string> vault_opt = vault_address_.empty() ?
        std::nullopt : std::optional<std::string>(vault_address_);
    auto signature = signL1Action(*wallet_, action, vault_opt, timestamp,
                                 expires_after_, is_mainnet);

    return postAction(action, signature, timestamp);
}

nlohmann::json Exchange::usdTransfer(double amount, const std::string& destination) {
    nlohmann::json action = {
        {"type", "usdSend"},
        {"destination", destination},
        {"amount", floatToWire(amount)},
        {"time", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"amount", "string"},
        {"time", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:UsdSend",
                                         is_mainnet);

    return postAction(action, signature, action["time"]);
}

nlohmann::json Exchange::spotTransfer(double amount,
                                      const std::string& destination,
                                      const std::string& token) {
    nlohmann::json action = {
        {"type", "spotSend"},
        {"destination", destination},
        {"token", token},
        {"amount", floatToWire(amount)},
        {"time", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"token", "string"},
        {"amount", "string"},
        {"time", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:SpotSend",
                                         is_mainnet);

    return postAction(action, signature, action["time"]);
}

nlohmann::json Exchange::updateLeverage(int leverage,
                                        const std::string& coin,
                                        bool is_cross) {
    int asset = info_.nameToAsset(coin);

    nlohmann::ordered_json leverage_obj;
    if (is_cross) {
        leverage_obj["type"] = "cross";
        leverage_obj["value"] = leverage;
    } else {
        leverage_obj["type"] = "isolated";
        leverage_obj["value"] = leverage;
    }

    nlohmann::ordered_json action;
    action["type"] = "updateLeverage";
    action["asset"] = asset;
    action["isCross"] = is_cross;
    action["leverage"] = leverage;

    int64_t timestamp = getTimestampMs();
    bool is_mainnet = (base_url_ == MAINNET_API_URL);

    std::optional<std::string> vault_opt = vault_address_.empty() ?
        std::nullopt : std::optional<std::string>(vault_address_);
    auto signature = signL1Action(*wallet_, action, vault_opt, timestamp,
                                 expires_after_, is_mainnet);

    return postAction(action, signature, timestamp);
}

} // namespace hyperliquid
