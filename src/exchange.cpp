#include "hyperliquid/exchange.hpp"
#include "hyperliquid/utils/constants.hpp"
#include "hyperliquid/utils/conversions.hpp"
#include <openssl/rand.h>
#include <algorithm>
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
// vaultTransfer belongs here for a different reason than the rest: it names its
// target vault inside the action, so a configured vault in the envelope would
// be a second, contradictory one. The Python SDK signs it with no vault and
// sends it with one, same mismatch as below.
//
// HIP-4 deployer actions belong to the deployer account, like the Python SDK's
// spot/perp deploy actions, which it signs with no vault.
//
// ponytail: a plain list, a lookup table does not pay for itself at seven entries.
static bool actionIgnoresVault(const std::string& action_type) {
    return action_type == "setReferrer" ||
           action_type == "createSubAccount" ||
           action_type == "subAccountTransfer" ||
           action_type == "subAccountSpotTransfer" ||
           action_type == "vaultTransfer" ||
           action_type == "activateOutcomeDeployer" ||
           action_type == "outcomeDeploy";
}

// Lot size. Outcomes are listed in no meta response and trade whole shares, so
// they are the one asset class without an entry. Every other name was already
// resolved by nameToAsset(), which throws for unknown ones.
static int szDecimalsFor(const Info& info, int asset) {
    auto it = info.asset_to_sz_decimals_.find(asset);
    return it == info.asset_to_sz_decimals_.end() ? 0 : it->second;
}

// Tick size. HIP-3 perps (110000+) are perps, not spot, despite sitting above
// the spot range: the Python SDK's `asset >= 10_000` gives them spot's 8
// decimals, which the exchange rejects for 7+ decimal prices.
static double roundPriceFor(double px, int asset, int sz_decimals) {
    if (asset >= OUTCOME_ASSET_OFFSET) {
        return roundOutcomePrice(px);
    }
    const bool is_spot = asset >= 10000 && asset < 100000;
    return roundPrice(px, sz_decimals, is_spot);
}

static OrderRequest roundedToTickAndLot(const Info& info, OrderRequest order, int asset) {
    const int sz_decimals = szDecimalsFor(info, asset);
    order.limit_px = roundPriceFor(order.limit_px, asset, sz_decimals);
    order.sz = roundSize(order.sz, sz_decimals);
    return order;
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

nlohmann::json Exchange::vaultTransfer(const std::string& vault_address,
                                      bool is_deposit,
                                      double usd) {
    if (usd <= 0.0) {
        throw std::invalid_argument("vaultTransfer usd must be positive; "
                                    "use is_deposit = false to withdraw");
    }

    nlohmann::ordered_json action;
    action["type"] = "vaultTransfer";
    action["vaultAddress"] = vault_address;
    action["isDeposit"] = is_deposit;
    // Wire format is an integer with 6 decimals, the same encoding
    // updateIsolatedMargin() uses for ntli.
    action["usd"] = floatToUsdInt(usd);

    return postL1Action(action);
}

nlohmann::json Exchange::withdrawFromBridge(double amount, const std::string& destination) {
    nlohmann::json action = {
        {"type", "withdraw3"},
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
                                         "HyperliquidTransaction:Withdraw",
                                         is_mainnet);

    // User-signed action: the nonce is the "time" already inside the action, so
    // the two cannot disagree.
    return postAction(action, signature, action["time"]);
}

void Exchange::useWebsocketTransport(bool enabled) {
    info_.useWebsocketTransport(enabled);
    // Adopt whatever info_ settled on rather than deciding again here, so the
    // two halves of the client cannot end up on different sockets.
    ws_transport_ = info_.websocketTransport();
}

double Exchange::slippagePrice(const std::string& name,
                              bool is_buy,
                              double slippage,
                              std::optional<double> px) {
    std::string coin = info_.nameToCoin(name);
    int asset = info_.nameToAsset(name);

    // Get mid price if not provided
    if (!px.has_value()) {
        auto mids = info_.allMids("");
        px = std::stod(mids.at(coin).get<std::string>());
    }

    // Calculate slippage
    double price = px.value();
    price *= is_buy ? (1.0 + slippage) : (1.0 - slippage);

    // An outcome side pays at most 1, so its price lives strictly inside
    // (0, 1); slippage off a 0.98 mid would otherwise ask for 1.029.
    if (asset >= OUTCOME_ASSET_OFFSET) {
        price = std::clamp(price, 0.00001, 0.99999);
    }

    // Round to tick size (5 significant figures and MAX_DECIMALS - szDecimals)
    return roundPriceFor(price, asset, szDecimalsFor(info_, asset));
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
    // bulkOrders() rounds to tick/lot size.
    OrderRequest order_req;
    order_req.coin = coin;
    order_req.is_buy = is_buy;
    order_req.sz = sz;
    order_req.limit_px = limit_px;
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
        order_wires.push_back(
            orderRequestToOrderWire(roundedToTickAndLot(info_, order, asset), asset));
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
        // Its own action, not "cancel" with a cloid in the oid slot: the wire
        // names are spelled out here ("asset"/"cloid", not "a"/"o"), and the
        // exchange types the "cancel" action's o as a uint64, so a cloid string
        // there fails to deserialize before it ever reaches an order book.
        nlohmann::ordered_json cancel_obj;
        cancel_obj["asset"] = info_.nameToAsset(cancel.coin);
        cancel_obj["cloid"] = cancel.cloid.toRaw();
        cancels_array.push_back(cancel_obj);
    }

    nlohmann::ordered_json action;
    action["type"] = "cancelByCloid";
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
    // bulkModifyOrders() rounds to tick/lot size.
    ModifyRequest modify_req;
    modify_req.oid = oid;
    modify_req.order.coin = coin;
    modify_req.order.is_buy = is_buy;
    modify_req.order.sz = sz;
    modify_req.order.limit_px = limit_px;
    modify_req.order.order_type = order_type;
    modify_req.order.reduce_only = reduce_only;
    modify_req.order.cloid = cloid;

    return bulkModifyOrders({modify_req});
}

nlohmann::json Exchange::bulkModifyOrders(const std::vector<ModifyRequest>& modifies) {
    nlohmann::ordered_json modifies_array = nlohmann::ordered_json::array();
    for (const auto& modify : modifies) {
        int asset = info_.nameToAsset(modify.order.coin);
        OrderWire wire = orderRequestToOrderWire(
            roundedToTickAndLot(info_, modify.order, asset), asset);

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
    int sz_decimals = szDecimalsFor(info_, asset);

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

// --- HIP-4 outcomes ----------------------------------------------------------
//
// Field order below is the order the docs give, which is what gets hashed:
// these are L1 actions, signed over their msgpack encoding.

static nlohmann::ordered_json userOutcomeAction(const char* variant, nlohmann::ordered_json body) {
    nlohmann::ordered_json action;
    action["type"] = "userOutcome";
    action[variant] = std::move(body);
    return action;
}

static void requirePositive(double amount, const char* what) {
    if (!(amount > 0.0)) {
        throw std::invalid_argument(std::string(what) + " amount must be positive");
    }
}

// nullopt means "as much as possible", which the wire spells null.
static nlohmann::ordered_json optionalAmount(std::optional<double> amount, const char* what) {
    if (!amount.has_value()) {
        return nullptr;
    }
    requirePositive(*amount, what);
    return floatToWire(*amount);
}

nlohmann::json Exchange::splitOutcome(int64_t outcome, double amount) {
    requirePositive(amount, "splitOutcome");

    nlohmann::ordered_json body;
    body["outcome"] = outcome;
    body["amount"] = floatToWire(amount);
    return postL1Action(userOutcomeAction("splitOutcome", std::move(body)));
}

nlohmann::json Exchange::mergeOutcome(int64_t outcome, std::optional<double> amount) {
    nlohmann::ordered_json body;
    body["outcome"] = outcome;
    body["amount"] = optionalAmount(amount, "mergeOutcome");
    return postL1Action(userOutcomeAction("mergeOutcome", std::move(body)));
}

nlohmann::json Exchange::mergeQuestion(int64_t question, std::optional<double> amount) {
    nlohmann::ordered_json body;
    body["question"] = question;
    body["amount"] = optionalAmount(amount, "mergeQuestion");
    return postL1Action(userOutcomeAction("mergeQuestion", std::move(body)));
}

nlohmann::json Exchange::negateOutcome(int64_t question, int64_t outcome, double amount) {
    requirePositive(amount, "negateOutcome");

    nlohmann::ordered_json body;
    body["question"] = question;
    body["outcome"] = outcome;
    body["amount"] = floatToWire(amount);
    return postL1Action(userOutcomeAction("negateOutcome", std::move(body)));
}

nlohmann::json Exchange::activateOutcomeDeployer(const std::string& venue_name) {
    if (venue_name.empty()) {
        throw std::invalid_argument("activateOutcomeDeployer venue name must not be empty");
    }

    nlohmann::ordered_json action;
    action["type"] = "activateOutcomeDeployer";
    action["activate"] = {{"venueName", venue_name}};
    return postL1Action(action);
}

nlohmann::json Exchange::deactivateOutcomeDeployer() {
    nlohmann::ordered_json action;
    action["type"] = "activateOutcomeDeployer";
    action["deactivate"] = nullptr;
    return postL1Action(action);
}

nlohmann::json Exchange::postOutcomeDeploy(const std::string& venue,
                                           const char* variant,
                                           nlohmann::ordered_json body) {
    if (venue.empty()) {
        throw std::invalid_argument(std::string(variant) + " requires the deployer's venue");
    }

    nlohmann::ordered_json operation;
    operation[variant] = std::move(body);

    nlohmann::ordered_json action;
    action["type"] = "outcomeDeploy";
    action["venue"] = venue;
    action["operation"] = std::move(operation);
    return postL1Action(action);
}

static nlohmann::ordered_json stringPair(const std::pair<std::string, std::string>& p) {
    return nlohmann::ordered_json::array({p.first, p.second});
}

// {"id", "keywordToValue": [[k, v], ...]}; the map keeps keywords sorted.
static nlohmann::ordered_json templateInstanceWire(const OutcomeTemplateInstance& instance) {
    nlohmann::ordered_json keyword_to_value = nlohmann::ordered_json::array();
    for (const auto& kv : instance.keyword_to_value) {
        keyword_to_value.push_back(stringPair(kv));
    }

    nlohmann::ordered_json wire;
    wire["id"] = instance.id;
    wire["keywordToValue"] = std::move(keyword_to_value);
    return wire;
}

static nlohmann::ordered_json settlementWire(const OutcomeSettlement& settlement) {
    nlohmann::ordered_json wire;
    wire["outcome"] = settlement.outcome;
    wire["settleFraction"] = floatToWire(settlement.settle_fraction);
    wire["details"] = "";  // the exchange requires it empty
    wire["nameAndDescription"] = stringPair(settlement.name_and_description);
    wire["sideNames"] = stringPair(settlement.side_names);
    return wire;
}

nlohmann::json Exchange::registerStandaloneOutcomeFromTemplate(const std::string& venue,
                                                               const OutcomeTemplateInstance& instance,
                                                               double deployer_fee_scale) {
    auto body = templateInstanceWire(instance);
    body["deployerFeeScale"] = floatToWire(deployer_fee_scale);
    return postOutcomeDeploy(venue, "registerStandaloneOutcomeFromTemplate", std::move(body));
}

nlohmann::json Exchange::registerQuestionFromTemplate(
    const std::string& venue,
    const OutcomeTemplateInstance& question,
    double deployer_fee_scale,
    const std::vector<OutcomeTemplateInstance>& named_outcomes) {
    auto question_wire = templateInstanceWire(question);
    question_wire["deployerFeeScale"] = floatToWire(deployer_fee_scale);

    nlohmann::ordered_json named = nlohmann::ordered_json::array();
    for (const auto& instance : named_outcomes) {
        named.push_back(templateInstanceWire(instance));
    }

    nlohmann::ordered_json body;
    body["questionTemplateInstance"] = std::move(question_wire);
    body["namedOutcomeTemplateInstances"] = std::move(named);
    return postOutcomeDeploy(venue, "registerQuestionFromTemplate", std::move(body));
}

nlohmann::json Exchange::registerAndAssociateNamedOutcomeFromTemplate(
    const std::string& venue,
    int64_t question,
    const OutcomeTemplateInstance& named_outcome) {
    nlohmann::ordered_json body;
    body["question"] = question;
    body["namedOutcomeTemplateInstance"] = templateInstanceWire(named_outcome);
    return postOutcomeDeploy(venue, "registerAndAssociateNamedOutcomeFromTemplate", std::move(body));
}

nlohmann::json Exchange::settleOutcome(const std::string& venue, const OutcomeSettlement& settlement) {
    return postOutcomeDeploy(venue, "settleOutcome", settlementWire(settlement));
}

nlohmann::json Exchange::settleQuestion(const std::string& venue,
                                        int64_t question,
                                        const std::vector<OutcomeSettlement>& settlements,
                                        const std::pair<std::string, std::string>& name_and_description) {
    if (settlements.empty()) {
        throw std::invalid_argument("settleQuestion requires the question's remaining outcomes");
    }

    nlohmann::ordered_json settlements_wire = nlohmann::ordered_json::array();
    for (const auto& settlement : settlements) {
        settlements_wire.push_back(settlementWire(settlement));
    }

    nlohmann::ordered_json body;
    body["question"] = question;
    body["outcomeSettlements"] = std::move(settlements_wire);
    body["nameAndDescription"] = stringPair(name_and_description);
    return postOutcomeDeploy(venue, "settleQuestion2", std::move(body));
}

nlohmann::json Exchange::setOutcomeSubDeployers(const std::string& venue,
                                                const std::vector<OutcomeSubDeployer>& changes) {
    if (changes.empty()) {
        throw std::invalid_argument("setOutcomeSubDeployers requires at least one change");
    }

    nlohmann::ordered_json body = nlohmann::ordered_json::array();
    for (const auto& change : changes) {
        nlohmann::ordered_json entry;
        entry["variant"] = change.variant;
        entry["user"] = normalizeAddress(change.user);
        entry["allowed"] = change.allowed;
        body.push_back(std::move(entry));
    }
    return postOutcomeDeploy(venue, "setSubDeployers", std::move(body));
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

// Staking. All three are user-signed, so each carries its own nonce and no
// vaultAddress field. The docs publish the action bodies but not the EIP-712
// type lists; for every user-signed action the list is the action's fields
// minus "type" and "signatureChainId", with "hyperliquidChain" prepended,
// which is what the shipped seven and the Python SDK's tokenDelegate all do.
static nlohmann::json stakingBalanceAction(const char* type, uint64_t wei) {
    if (wei == 0) {
        throw std::invalid_argument(std::string(type) + " wei must be positive");
    }

    return nlohmann::json{
        {"type", type},
        {"wei", wei},
        {"nonce", getTimestampMs()}
    };
}

nlohmann::json Exchange::cDeposit(uint64_t wei) {
    auto action = stakingBalanceAction("cDeposit", wei);

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"wei", "uint64"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:CDeposit",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

nlohmann::json Exchange::cWithdraw(uint64_t wei) {
    auto action = stakingBalanceAction("cWithdraw", wei);

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"wei", "uint64"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:CWithdraw",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
}

nlohmann::json Exchange::tokenDelegate(const std::string& validator,
                                       uint64_t wei,
                                       bool is_undelegate) {
    if (wei == 0) {
        throw std::invalid_argument("tokenDelegate wei must be positive");
    }

    nlohmann::json action = {
        {"type", "tokenDelegate"},
        {"validator", normalizeAddress(validator)},
        {"wei", wei},
        {"isUndelegate", is_undelegate},
        {"nonce", getTimestampMs()}
    };

    std::vector<EIP712Type> payload_types = {
        {"hyperliquidChain", "string"},
        {"validator", "address"},
        {"wei", "uint64"},
        {"isUndelegate", "bool"},
        {"nonce", "uint64"}
    };

    bool is_mainnet = (base_url_ == MAINNET_API_URL);
    auto signature = signUserSignedAction(*wallet_, action, payload_types,
                                         "HyperliquidTransaction:TokenDelegate",
                                         is_mainnet);

    return postAction(action, signature, action["nonce"]);
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
