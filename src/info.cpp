#include "hyperliquid/info.hpp"
#include "hyperliquid/errors.hpp"
#include "hyperliquid/utils/constants.hpp"
#include <stdexcept>

#ifdef HYPERLIQUID_WEBSOCKET
#include "hyperliquid/websocket_manager.hpp"
#endif

namespace hyperliquid {

Info::Info(const std::string& base_url,
          bool skip_ws,
          const Meta* meta,
          const SpotMeta* spot_meta,
          const std::vector<std::string>* perp_dexs,
          int timeout_ms,
          std::shared_ptr<detail::HttpConnection> connection)
    : API(base_url.empty() ? MAINNET_API_URL : base_url, timeout_ms,
          std::move(connection)) {
    initializeMetadata(meta, spot_meta, perp_dexs);

#ifdef HYPERLIQUID_WEBSOCKET
    if (!skip_ws) {
        ws_manager_ = std::make_unique<WebSocketManager>(base_url_);
        ws_manager_->start();
    }
#else
    if (!skip_ws) {
        throw Error("this build has no websocket support; rebuild with -DBUILD_WEBSOCKET=ON");
    }
#endif
}

// Defined here, where WebSocketManager is a complete type, so ~unique_ptr can
// see its destructor.
Info::~Info() = default;

#ifdef HYPERLIQUID_WEBSOCKET

WebSocketManager& Info::requireWebSocket() const {
    if (!ws_manager_) {
        throw Error("websocket not enabled; construct Info with skip_ws = false");
    }
    return *ws_manager_;
}

int Info::subscribe(const nlohmann::json& subscription,
                    std::function<void(const nlohmann::json&)> callback) {
    return requireWebSocket().subscribe(subscription, std::move(callback));
}

bool Info::unsubscribe(const nlohmann::json& subscription, int subscription_id) {
    return requireWebSocket().unsubscribe(subscription, subscription_id);
}

void Info::disconnectWebsocket() {
    if (ws_manager_) {
        ws_manager_->stop();
    }
}

#else

// requireWebSocket() is intentionally left undefined here: nothing can call it
// when there is no manager to return.
namespace {
constexpr const char* kNoWebSocketBuild =
    "this build has no websocket support; rebuild with -DBUILD_WEBSOCKET=ON";
}

int Info::subscribe(const nlohmann::json&, std::function<void(const nlohmann::json&)>) {
    throw Error(kNoWebSocketBuild);
}

bool Info::unsubscribe(const nlohmann::json&, int) {
    throw Error(kNoWebSocketBuild);
}

void Info::disconnectWebsocket() {}

#endif

void Info::initializeMetadata(const Meta* meta,
                              const SpotMeta* spot_meta,
                              const std::vector<std::string>* perp_dexs) {
    // Auto-fetch spot metadata if not provided (matches Python SDK behavior)
    SpotMeta spot_meta_obj;
    if (spot_meta) {
        spot_meta_obj = *spot_meta;
    } else {
        spot_meta_obj = spotMeta();
    }

    // A pair's "tokens" are token *ids*, not positions in the tokens array.
    // The two coincide for the first few hundred tokens and then stop: on
    // mainnet 21 tokens have index != position, and pairs reference ids past
    // the end of the array entirely (max id 852 against 479 tokens). Indexing
    // positionally therefore read out of bounds for 18 mainnet pairs and
    // silently picked the wrong token for others, giving that pair the wrong
    // szDecimals and so the wrong tick/lot rounding.
    //
    // Keyed lookup, as the Python SDK does (info.py: token_by_index).
    std::unordered_map<int, const SpotTokenInfo*> token_by_index;
    token_by_index.reserve(spot_meta_obj.tokens.size());
    for (const auto& token : spot_meta_obj.tokens) {
        token_by_index[token.index] = &token;
    }

    auto require_token = [&](int token_id, const std::string& pair_name) -> const SpotTokenInfo& {
        auto it = token_by_index.find(token_id);
        if (it == token_by_index.end()) {
            throw Error("spot pair " + pair_name + " references unknown token id " +
                        std::to_string(token_id) + "; spot metadata is inconsistent");
        }
        return *it->second;
    };

    // Add spot pairs (matches Python SDK logic)
    for (const auto& pair : spot_meta_obj.universe) {
        int asset = 10000 + pair.index;

        // Register pair name (e.g., "@107")
        coin_to_asset_[pair.name] = asset;
        name_to_coin_[pair.name] = pair.name;

        // Python unpacks exactly two ids here and would raise otherwise.
        if (pair.tokens.size() < 2) {
            throw Error("spot pair " + pair.name + " has fewer than two tokens");
        }

        const auto& base_token = require_token(pair.tokens[0], pair.name);
        const auto& quote_token = require_token(pair.tokens[1], pair.name);

        // Set sz_decimals to the BASE token's sz_decimals (critical for tick/lot size)
        asset_to_sz_decimals_[asset] = base_token.sz_decimals;

        // Also register by "BASE/QUOTE" name format
        std::string pair_format = base_token.name + "/" + quote_token.name;
        if (name_to_coin_.find(pair_format) == name_to_coin_.end()) {
            name_to_coin_[pair_format] = pair.name;
        }
    }

    // Auto-fetch perp metadata if not provided (matches Python SDK behavior)
    std::vector<std::string> dexs;
    if (perp_dexs) {
        dexs = *perp_dexs;
    } else {
        dexs = {""};  // Default to empty string (main dex)
    }

    for (size_t i = 0; i < dexs.size(); ++i) {
        int offset = 0;
        if (i > 0) {
            // Builder-deployed perps start at 110000
            offset = 110000 + static_cast<int>(i - 1) * 10000;
        }

        Meta perp_meta_obj;
        if (dexs[i].empty() && meta) {
            // Use provided meta for default dex
            perp_meta_obj = *meta;
        } else {
            // Auto-fetch metadata for this dex
            perp_meta_obj = this->meta(dexs[i]);
        }

        setPerpMeta(perp_meta_obj, offset);
    }
}

void Info::setPerpMeta(const Meta& meta, int offset) {
    for (size_t i = 0; i < meta.universe.size(); ++i) {
        const auto& asset = meta.universe[i];
        int asset_id = offset + static_cast<int>(i);

        coin_to_asset_[asset.name] = asset_id;
        name_to_coin_[asset.name] = asset.name;
        asset_to_sz_decimals_[asset_id] = asset.sz_decimals;
    }
}

void Info::registerPerpMeta(const Meta& meta, int offset) {
    setPerpMeta(meta, offset);
}

void Info::registerSpotMeta(const SpotMeta& spot_meta) {
    // Add spot pairs (matches Python SDK logic)
    for (const auto& pair : spot_meta.universe) {
        int asset = 10000 + pair.index;

        // Register pair name (e.g., "@107")
        coin_to_asset_[pair.name] = asset;
        name_to_coin_[pair.name] = pair.name;

        // Get base and quote token info
        int base_idx = pair.tokens[0];
        int quote_idx = pair.tokens[1];
        const auto& base_token = spot_meta.tokens[base_idx];
        const auto& quote_token = spot_meta.tokens[quote_idx];

        // Set sz_decimals to the BASE token's sz_decimals (critical for tick/lot size)
        asset_to_sz_decimals_[asset] = base_token.sz_decimals;

        // Also register by "BASE/QUOTE" name format
        std::string pair_format = base_token.name + "/" + quote_token.name;
        if (name_to_coin_.find(pair_format) == name_to_coin_.end()) {
            name_to_coin_[pair_format] = pair.name;
        }
    }
}

int Info::nameToAsset(const std::string& name) const {
    auto it = name_to_coin_.find(name);
    if (it == name_to_coin_.end()) {
        throw std::runtime_error("Unknown asset name: " + name);
    }

    std::string coin = it->second;
    auto asset_it = coin_to_asset_.find(coin);
    if (asset_it == coin_to_asset_.end()) {
        throw std::runtime_error("Unknown coin: " + coin);
    }

    return asset_it->second;
}

const std::string& Info::nameToCoin(const std::string& name) const {
    auto it = name_to_coin_.find(name);
    if (it == name_to_coin_.end()) {
        throw std::runtime_error("Unknown asset name: " + name);
    }
    return it->second;
}

nlohmann::json Info::userState(const std::string& address, const std::string& dex) {
    nlohmann::json payload = {
        {"type", "clearinghouseState"},
        {"user", address}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::spotUserState(const std::string& address) {
    nlohmann::json payload = {
        {"type", "spotClearinghouseState"},
        {"user", address}
    };
    return post("/info", payload);
}

nlohmann::json Info::openOrders(const std::string& address, const std::string& dex) {
    nlohmann::json payload = {
        {"type", "openOrders"},
        {"user", address}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::frontendOpenOrders(const std::string& address, const std::string& dex) {
    nlohmann::json payload = {
        {"type", "frontendOpenOrders"},
        {"user", address}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::allMids(const std::string& dex) {
    nlohmann::json payload = {
        {"type", "allMids"}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::userFills(const std::string& address) {
    nlohmann::json payload = {
        {"type", "userFills"},
        {"user", address}
    };
    return post("/info", payload);
}

nlohmann::json Info::userFillsByTime(const std::string& address,
                                     int64_t start_time,
                                     std::optional<int64_t> end_time) {
    nlohmann::json payload = {
        {"type", "userFillsByTime"},
        {"user", address},
        {"startTime", start_time}
    };
    if (end_time.has_value()) {
        payload["endTime"] = end_time.value();
    }
    return post("/info", payload);
}

Meta Info::meta(const std::string& dex) {
    nlohmann::json payload = {
        {"type", "meta"}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }

    auto response = post("/info", payload);

    Meta result;
    for (const auto& asset : response["universe"]) {
        AssetInfo info;
        info.name = asset["name"];
        info.sz_decimals = asset["szDecimals"];
        // Everything below is dex-dependent: the default dex omits growthMode,
        // and delisted/margin-mode fields only appear when they apply.
        info.max_leverage = asset.value("maxLeverage", 0);
        info.only_isolated = asset.value("onlyIsolated", false);
        info.is_delisted = asset.value("isDelisted", false);
        if (asset.contains("marginTableId")) {
            info.margin_table_id = asset["marginTableId"].get<int>();
        }
        if (asset.contains("marginMode")) {
            info.margin_mode = asset["marginMode"].get<std::string>();
        }
        if (asset.contains("growthMode")) {
            info.growth_mode = asset["growthMode"].get<std::string>();
        }
        if (asset.contains("lastGrowthModeChangeTime")) {
            info.last_growth_mode_change_time =
                asset["lastGrowthModeChangeTime"].get<std::string>();
        }
        result.universe.push_back(info);
    }

    // marginTables is a list of [id, table] pairs, not an object, so the id
    // lives outside the table body and is folded into MarginTable here.
    for (const auto& entry : response.value("marginTables", nlohmann::json::array())) {
        MarginTable table;
        table.id = entry.at(0).get<int>();

        const auto& body = entry.at(1);
        table.description = body.value("description", "");
        for (const auto& tier : body.value("marginTiers", nlohmann::json::array())) {
            MarginTier margin_tier;
            margin_tier.lower_bound = tier["lowerBound"];
            margin_tier.max_leverage = tier["maxLeverage"];
            table.margin_tiers.push_back(margin_tier);
        }
        result.margin_tables.push_back(table);
    }

    result.collateral_token = response.value("collateralToken", 0);

    return result;
}

SpotMeta Info::spotMeta() {
    nlohmann::json payload = {
        {"type", "spotMeta"}
    };

    auto response = post("/info", payload);

    SpotMeta result;

    // Parse tokens
    for (const auto& token : response["tokens"]) {
        SpotTokenInfo info;
        info.name = token["name"];
        info.sz_decimals = token["szDecimals"];
        info.wei_decimals = token["weiDecimals"];
        info.index = token["index"];
        info.token_id = token["tokenId"];
        info.is_canonical = token["isCanonical"];
        result.tokens.push_back(info);
    }

    // Parse universe
    for (const auto& asset : response["universe"]) {
        SpotAssetInfo info;
        info.name = asset["name"];
        info.tokens = asset["tokens"].get<std::vector<int>>();
        info.index = asset["index"];
        info.is_canonical = asset["isCanonical"];
        result.universe.push_back(info);
    }

    return result;
}

nlohmann::json Info::l2Snapshot(const std::string& name) {
    nlohmann::json payload = {
        {"type", "l2Book"},
        {"coin", name}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryOrderByOid(const std::string& user, int64_t oid) {
    nlohmann::json payload = {
        {"type", "orderStatus"},
        {"user", user},
        {"oid", oid}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryOrderByCloid(const std::string& user, const Cloid& cloid) {
    nlohmann::json payload = {
        {"type", "orderStatus"},
        {"user", user},
        {"oid", cloid.toRaw()}
    };
    return post("/info", payload);
}

nlohmann::json Info::candlesSnapshot(const std::string& name,
                                     const std::string& interval,
                                     int64_t start_time,
                                     std::optional<int64_t> end_time) {
    const std::string& coin = nameToCoin(name);
    nlohmann::json req = {
        {"coin", coin},
        {"interval", interval},
        {"startTime", start_time}
    };
    if (end_time.has_value()) {
        req["endTime"] = end_time.value();
    }
    nlohmann::json payload = {
        {"type", "candleSnapshot"},
        {"req", req}
    };
    return post("/info", payload);
}

nlohmann::json Info::userTwapSliceFills(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userTwapSliceFills"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::userFees(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userFees"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::predictedFundings() {
    nlohmann::json payload = {
        {"type", "predictedFundings"}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpsAtOpenInterestCap(const std::string& dex) {
    nlohmann::json payload = {
        {"type", "perpsAtOpenInterestCap"}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::perpCategories() {
    nlohmann::json payload = {
        {"type", "perpCategories"}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpConciseAnnotations() {
    nlohmann::json payload = {
        {"type", "perpConciseAnnotations"}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpAnnotation(const std::string& coin) {
    // The API answers an empty coin with null rather than an error, which is
    // indistinguishable from "this coin has no annotation".
    if (coin.empty()) {
        throw std::invalid_argument("perpAnnotation requires a coin name");
    }

    nlohmann::json payload = {
        {"type", "perpAnnotation"},
        {"coin", coin}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpDexLimits(const std::string& dex) {
    // Unlike most dex parameters, "" is not a shorthand for the default dex
    // here -- the default dex has no builder limits, and the API rejects it.
    if (dex.empty()) {
        throw std::invalid_argument("perpDexLimits requires a builder dex name");
    }

    nlohmann::json payload = {
        {"type", "perpDexLimits"},
        {"dex", dex}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpDexStatus(const std::string& dex) {
    nlohmann::json payload = {
        {"type", "perpDexStatus"},
        {"dex", dex}
    };
    return post("/info", payload);
}

nlohmann::json Info::tokenDetails(const std::string& token_id) {
    nlohmann::json payload = {
        {"type", "tokenDetails"},
        {"tokenId", token_id}
    };
    return post("/info", payload);
}

nlohmann::json Info::maxBuilderFee(const std::string& user, const std::string& builder) {
    nlohmann::json payload = {
        {"type", "maxBuilderFee"},
        {"user", user},
        {"builder", builder}
    };
    return post("/info", payload);
}

nlohmann::json Info::historicalOrders(const std::string& user) {
    nlohmann::json payload = {
        {"type", "historicalOrders"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::fundingHistory(const std::string& name,
                                    int64_t start_time,
                                    std::optional<int64_t> end_time) {
    const std::string& coin = nameToCoin(name);
    nlohmann::json payload = {
        {"type", "fundingHistory"},
        {"coin", coin},
        {"startTime", start_time}
    };
    if (end_time.has_value()) {
        payload["endTime"] = end_time.value();
    }
    return post("/info", payload);
}

nlohmann::json Info::querySubAccounts(const std::string& user) {
    nlohmann::json payload = {
        {"type", "subAccounts"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryReferralState(const std::string& user) {
    nlohmann::json payload = {
        {"type", "referral"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::approvedBuilders(const std::string& user) {
    nlohmann::json payload = {
        {"type", "approvedBuilders"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::userRole(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userRole"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::userRateLimit(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userRateLimit"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::portfolio(const std::string& user) {
    nlohmann::json payload = {
        {"type", "portfolio"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::userNonFundingLedgerUpdates(const std::string& user,
                                                  int64_t start_time,
                                                  std::optional<int64_t> end_time) {
    nlohmann::json payload = {
        {"type", "userNonFundingLedgerUpdates"},
        {"user", user},
        {"startTime", start_time}
    };
    if (end_time.has_value()) {
        payload["endTime"] = end_time.value();
    }
    return post("/info", payload);
}

nlohmann::json Info::vaultDetails(const std::string& vault_address,
                                   const std::string& user) {
    nlohmann::json payload = {
        {"type", "vaultDetails"},
        {"vaultAddress", vault_address}
    };
    if (!user.empty()) {
        payload["user"] = user;
    }
    return post("/info", payload);
}

nlohmann::json Info::userVaultEquities(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userVaultEquities"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryUserDexAbstractionState(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userDexAbstraction"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryUserAbstractionState(const std::string& user) {
    nlohmann::json payload = {
        {"type", "userAbstraction"},
        {"user", user}
    };
    return post("/info", payload);
}

nlohmann::json Info::metaAndAssetCtxs(const std::string& dex) {
    nlohmann::json payload = {
        {"type", "metaAndAssetCtxs"}
    };
    if (!dex.empty()) {
        payload["dex"] = dex;
    }
    return post("/info", payload);
}

nlohmann::json Info::spotMetaAndAssetCtxs() {
    nlohmann::json payload = {
        {"type", "spotMetaAndAssetCtxs"}
    };
    return post("/info", payload);
}

nlohmann::json Info::perpDexs() {
    nlohmann::json payload = {
        {"type", "perpDexs"}
    };
    return post("/info", payload);
}

nlohmann::json Info::queryPerpDeployAuctionStatus() {
    nlohmann::json payload = {
        {"type", "perpDeployAuctionStatus"}
    };
    return post("/info", payload);
}

nlohmann::json Info::userFundingHistory(const std::string& user,
                                        int64_t start_time,
                                        std::optional<int64_t> end_time) {
    nlohmann::json payload = {
        {"type", "userFunding"},
        {"user", user},
        {"startTime", start_time}
    };
    if (end_time.has_value()) {
        payload["endTime"] = end_time.value();
    }
    return post("/info", payload);
}

} // namespace hyperliquid
