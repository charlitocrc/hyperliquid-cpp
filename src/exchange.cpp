#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/constants.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <openssl/rand.h>
#include <cmath>
#include <stdexcept>

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
      // Share this Exchange's connection rather than opening a second one to
      // the same host: base classes are constructed before members, so the
      // connection already exists. Saves a TLS handshake at startup, and keeps
      // the info path warm whenever the trading path is used, and vice versa.
      info_(base_url, true, meta, spot_meta, perp_dexs, timeout_ms, connection()),
      wallet_(wallet),
      vault_address_(vault_address),
      account_address_(account_address),
      expires_after_(std::nullopt) {
}

// Account-level actions that operate on the master account and never on a
// configured vault/subaccount, so the vault is neither signed over nor sent.
//
// The Python SDK signs these with no vault but still puts the configured vault
// in the envelope. The signature would then not cover the vault it is sent
// with, so the server cannot verify it; we omit it from both instead. Only
// reachable when an Exchange has a vault_address configured.
// ponytail: a plain list, a lookup table does not pay for itself at four entries.
static bool actionIgnoresVault(const std::string& action_type) {
    return action_type == "setReferrer" ||
           action_type == "createSubAccount" ||
           action_type == "subAccountTransfer" ||
           action_type == "subAccountSpotTransfer";
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
        if (!vault_address_.empty() && !actionIgnoresVault(action_type)) {
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

nlohmann::json Exchange::postL1Action(const nlohmann::ordered_json& action,
                                      std::optional<int64_t> nonce_override) {
    const int64_t nonce = nonce_override.value_or(getTimestampMs());

    std::optional<std::string> vault_opt;
    if (!vault_address_.empty() && !actionIgnoresVault(action["type"])) {
        vault_opt = vault_address_;
    }

    auto signature = signL1Action(*wallet_, action, vault_opt, nonce,
                                 expires_after_, base_url_ == MAINNET_API_URL);

    return postAction(action, signature, nonce);
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

    return postL1Action(orderWiresToOrderAction(order_wires, builder, grouping));
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

    return postL1Action(action);
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

    return postL1Action(action);
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

    return postL1Action(action);
}

nlohmann::json Exchange::twapOrder(const std::string& coin,
                                   bool is_buy,
                                   double sz,
                                   int minutes,
                                   bool reduce_only,
                                   bool randomize) {
    int asset = info_.nameToAsset(coin);
    int sz_decimals = info_.asset_to_sz_decimals_[asset];

    double rounded_sz = roundSize(sz, sz_decimals);
    if (rounded_sz <= 0.0) {
        throw std::invalid_argument(
            "TWAP size rounds to zero for " + coin +
            " (szDecimals=" + std::to_string(sz_decimals) + ")");
    }

    // Duration bounds are not published, so let the API own that policy rather
    // than guessing a range and rejecting orders it would have accepted.
    if (minutes <= 0) {
        throw std::invalid_argument("TWAP minutes must be positive");
    }

    TwapWire twap;
    twap.asset = asset;
    twap.is_buy = is_buy;
    twap.size = floatToWire(rounded_sz);
    twap.reduce_only = reduce_only;
    twap.minutes = minutes;
    twap.randomize = randomize;

    nlohmann::ordered_json action;
    action["type"] = "twapOrder";
    action["twap"] = twap.toJson();

    return postL1Action(action);
}

nlohmann::json Exchange::twapCancel(const std::string& coin, int64_t twap_id) {
    nlohmann::ordered_json action;
    action["type"] = "twapCancel";
    action["a"] = info_.nameToAsset(coin);
    action["t"] = twap_id;

    return postL1Action(action);
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

nlohmann::json Exchange::sendAsset(const std::string& destination,
                                   const std::string& source_dex,
                                   const std::string& destination_dex,
                                   const std::string& token,
                                   double amount) {
    nlohmann::json action = {
        {"type", "sendAsset"},
        {"destination", destination},
        {"sourceDex", source_dex},
        {"destinationDex", destination_dex},
        {"token", token},
        {"amount", floatToWire(amount)},
        // No vaultAddress field on this action (postAction omits it); acting
        // on behalf of a subaccount is a signed field instead.
        {"fromSubAccount", vault_address_},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"destination", "string"},
        {"sourceDex", "string"},
        {"destinationDex", "string"},
        {"token", "string"},
        {"amount", "string"},
        {"fromSubAccount", "string"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:SendAsset",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

nlohmann::json Exchange::usdClassTransfer(double amount, bool to_perp) {
    std::string str_amount = floatToWire(amount);
    // This action has no vaultAddress field (postAction omits it); acting on
    // behalf of a subaccount is encoded in the signed amount string instead.
    if (!vault_address_.empty()) {
        str_amount += " subaccount:" + vault_address_;
    }

    nlohmann::json action = {
        {"type", "usdClassTransfer"},
        {"amount", str_amount},
        {"toPerp", to_perp},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"amount", "string"},
        {"toPerp", "bool"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:UsdClassTransfer",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

std::pair<nlohmann::json, std::string> Exchange::approveAgent(
    const std::optional<std::string>& name) {
    // Generate the agent key locally, like the Python SDK: 32 random bytes
    // from the CSPRNG, returned to the caller as the only copy.
    std::vector<uint8_t> key_bytes(32);
    if (RAND_bytes(key_bytes.data(), static_cast<int>(key_bytes.size())) != 1) {
        throw std::runtime_error("Failed to generate agent private key");
    }
    std::string agent_key = bytesToHex(key_bytes, /*with_prefix=*/true);
    std::string agent_address = Wallet::fromPrivateKey(agent_key)->address();

    nlohmann::json action = {
        {"type", "approveAgent"},
        {"agentAddress", agent_address},
        {"agentName", name.value_or("")},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"agentAddress", "address"},
        {"agentName", "string"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:ApproveAgent",
                                         is_mainnet);

    // Signed with agentName "" but sent without it, matching the Python SDK.
    if (!name.has_value()) {
        action.erase("agentName");
    }

    return {postAction(action, signature, action["nonce"]), agent_key};
}

nlohmann::json Exchange::approveBuilderFee(const std::string& builder,
                                           const std::string& max_fee_rate) {
    nlohmann::json action = {
        {"type", "approveBuilderFee"},
        {"maxFeeRate", max_fee_rate},
        {"builder", builder},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"maxFeeRate", "string"},
        {"builder", "address"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:ApproveBuilderFee",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

nlohmann::json Exchange::convertToMultiSigUser(const std::vector<std::string>& authorized_users,
                                               int threshold) {
    nlohmann::json action = {
        {"type", "convertToMultiSigUser"},
        {"signers", convertToMultiSigUserSigners(authorized_users, threshold)},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"signers", "string"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:ConvertToMultiSigUser",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

nlohmann::json Exchange::multiSig(const std::string& multi_sig_user,
                                  const nlohmann::ordered_json& inner_action,
                                  const std::vector<Signature>& signatures,
                                  int64_t nonce) {
    if (signatures.empty()) {
        throw std::invalid_argument("multiSig requires at least one signature");
    }
    if (!inner_action.contains("type")) {
        throw std::invalid_argument("multiSig inner action must have a type field");
    }

    nlohmann::ordered_json signatures_array = nlohmann::ordered_json::array();
    for (const auto& signature : signatures) {
        signatures_array.push_back(nlohmann::ordered_json(signature.toJson()));
    }

    nlohmann::ordered_json payload;
    payload["multiSigUser"] = normalizeAddress(multi_sig_user);
    // The leader, which is this wallet: it sends the action, so its nonce is
    // the one the exchange validates and consumes.
    payload["outerSigner"] = normalizeAddress(wallet_->address());
    payload["action"] = inner_action;

    nlohmann::ordered_json action;
    action["type"] = "multiSig";
    action["signatureChainId"] = "0x66eee";
    action["signatures"] = signatures_array;
    action["payload"] = payload;

    // Unlike the Python SDK, which signs over a vault argument but ships the
    // Exchange's configured vault in the envelope, both come from the same
    // place here -- otherwise the signature covers a vault the request does not
    // carry, and the exchange cannot verify it.
    std::optional<std::string> vault_opt;
    if (!vault_address_.empty()) {
        vault_opt = vault_address_;
    }

    auto signature = signMultiSigAction(*wallet_, action, vault_opt, nonce,
                                       expires_after_, base_url_ == MAINNET_API_URL);

    return postAction(action, signature, nonce);
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

    return postL1Action(action);
}

nlohmann::json Exchange::updateIsolatedMargin(double amount, const std::string& coin) {
    int asset = info_.nameToAsset(coin);
    int64_t ntli = floatToUsdInt(amount);

    nlohmann::ordered_json action;
    action["type"] = "updateIsolatedMargin";
    action["asset"] = asset;
    action["isBuy"] = true;
    action["ntli"] = ntli;

    return postL1Action(action);
}

nlohmann::json Exchange::topUpIsolatedOnlyMargin(const std::string& coin, double leverage) {
    if (leverage <= 0.0) {
        throw std::invalid_argument("topUpIsolatedOnlyMargin leverage must be positive");
    }

    nlohmann::ordered_json action;
    action["type"] = "topUpIsolatedOnlyMargin";
    action["asset"] = info_.nameToAsset(coin);
    // The API wants leverage as a float string ("5" not 5.0); floatToWire
    // normalizes trailing zeros the same way order prices are encoded.
    action["leverage"] = floatToWire(leverage);

    return postL1Action(action);
}

nlohmann::json Exchange::setReferrer(const std::string& code) {
    if (code.empty()) {
        throw std::invalid_argument("setReferrer code must not be empty");
    }

    nlohmann::ordered_json action;
    action["type"] = "setReferrer";
    action["code"] = code;

    return postL1Action(action);
}

nlohmann::json Exchange::createSubAccount(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("createSubAccount name must not be empty");
    }

    nlohmann::ordered_json action;
    action["type"] = "createSubAccount";
    action["name"] = name;

    return postL1Action(action);
}

nlohmann::json Exchange::subAccountTransfer(const std::string& sub_account_user,
                                            bool is_deposit,
                                            int64_t usd) {
    if (usd <= 0) {
        throw std::invalid_argument("subAccountTransfer usd must be positive");
    }

    nlohmann::ordered_json action;
    action["type"] = "subAccountTransfer";
    action["subAccountUser"] = sub_account_user;
    action["isDeposit"] = is_deposit;
    action["usd"] = usd;

    return postL1Action(action);
}

nlohmann::json Exchange::subAccountSpotTransfer(const std::string& sub_account_user,
                                                bool is_deposit,
                                                const std::string& token,
                                                double amount) {
    if (token.empty()) {
        throw std::invalid_argument("subAccountSpotTransfer token must not be empty");
    }

    nlohmann::ordered_json action;
    action["type"] = "subAccountSpotTransfer";
    action["subAccountUser"] = sub_account_user;
    action["isDeposit"] = is_deposit;
    action["token"] = token;
    // Python sends str(amount); floatToWire is this SDK's equivalent and
    // throws rather than silently rounding past 8 decimals.
    action["amount"] = floatToWire(amount);

    return postL1Action(action);
}

nlohmann::json Exchange::noop(std::optional<int64_t> nonce) {
    nlohmann::ordered_json action;
    action["type"] = "noop";

    return postL1Action(action, nonce);
}

nlohmann::json Exchange::reserveRequestWeight(int64_t weight) {
    if (weight <= 0) {
        throw std::invalid_argument("reserveRequestWeight weight must be positive");
    }

    nlohmann::ordered_json action;
    action["type"] = "reserveRequestWeight";
    action["weight"] = weight;

    return postL1Action(action);
}

nlohmann::json Exchange::scheduleCancel(std::optional<int64_t> time) {
    nlohmann::ordered_json action;
    action["type"] = "scheduleCancel";
    if (time.has_value()) {
        action["time"] = time.value();
    }

    return postL1Action(action);
}

nlohmann::json Exchange::queryOrderByCloid(const std::string& user, const Cloid& cloid) {
    return info_.queryOrderByCloid(user, cloid);
}

} // namespace hyperliquid
