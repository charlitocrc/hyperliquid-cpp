#include "hyperliquid/info.hpp"
#include "hyperliquid/utils/constants.hpp"
#include <stdexcept>

namespace hyperliquid {

Info::Info(const std::string& base_url,
          bool skip_ws,
          const Meta* meta,
          const SpotMeta* spot_meta,
          const std::vector<std::string>* perp_dexs,
          int timeout_ms)
    : API(base_url.empty() ? MAINNET_API_URL : base_url, timeout_ms) {
    initializeMetadata(meta, spot_meta, perp_dexs);
}

void Info::initializeMetadata(const Meta* meta,
                              const SpotMeta* spot_meta,
                              const std::vector<std::string>* perp_dexs) {
    // Initialize perp metadata
    if (meta) {
        setPerpMeta(*meta, 0);
    } else {
        // Fetch from API
        auto fetched_meta = this->meta("");
        setPerpMeta(fetched_meta, 0);
    }

    // Initialize spot metadata
    if (spot_meta) {
        // Add spot tokens
        for (const auto& token : spot_meta->tokens) {
            asset_to_sz_decimals_[10000 + token.index] = token.sz_decimals;
        }
        // Add spot pairs
        for (const auto& pair : spot_meta->universe) {
            std::string pair_name = pair.name;
            coin_to_asset_[pair_name] = 10000 + pair.index;
            name_to_coin_[pair_name] = pair_name;
        }
    } else {
        // Fetch from API
        auto fetched_spot_meta = spotMeta();
        // Process fetched data similarly
    }

    // Handle builder-deployed perps if provided
    if (perp_dexs && !perp_dexs->empty()) {
        for (size_t i = 0; i < perp_dexs->size(); ++i) {
            if (!(*perp_dexs)[i].empty()) {
                auto dex_meta = this->meta((*perp_dexs)[i]);
                setPerpMeta(dex_meta, 110000 + static_cast<int>(i) * 10000);
            }
        }
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
        result.universe.push_back(info);
    }

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

} // namespace hyperliquid
