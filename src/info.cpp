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
    // Auto-fetch spot metadata if not provided (matches Python SDK behavior)
    SpotMeta spot_meta_obj;
    if (spot_meta) {
        spot_meta_obj = *spot_meta;
    } else {
        spot_meta_obj = spotMeta();
    }

    // Add spot pairs (matches Python SDK logic)
    for (const auto& pair : spot_meta_obj.universe) {
        int asset = 10000 + pair.index;

        // Register pair name (e.g., "@107")
        coin_to_asset_[pair.name] = asset;
        name_to_coin_[pair.name] = pair.name;

        // Get base and quote token info
        int base_idx = pair.tokens[0];
        int quote_idx = pair.tokens[1];
        const auto& base_token = spot_meta_obj.tokens[base_idx];
        const auto& quote_token = spot_meta_obj.tokens[quote_idx];

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
