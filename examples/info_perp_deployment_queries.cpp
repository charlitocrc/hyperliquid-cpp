#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <algorithm>
#include <iostream>
#include <string>

namespace {

void printFirstPerpAssetCtx(const nlohmann::json& meta_and_ctxs) {
    if (!meta_and_ctxs.is_array() || meta_and_ctxs.size() < 2 ||
        !meta_and_ctxs[0].contains("universe") || !meta_and_ctxs[1].is_array()) {
        std::cout << "  Response: " << meta_and_ctxs.dump(2) << "\n";
        return;
    }

    const auto& universe = meta_and_ctxs[0]["universe"];
    const auto& contexts = meta_and_ctxs[1];
    std::cout << "  Perp assets: " << universe.size() << "\n";

    if (!universe.empty() && !contexts.empty()) {
        const auto& asset = universe[0];
        const auto& ctx = contexts[0];
        std::cout << "  First asset: " << asset.value("name", "<unknown>")
                  << " | markPx: " << ctx.value("markPx", "n/a")
                  << " | funding: " << ctx.value("funding", "n/a")
                  << " | openInterest: " << ctx.value("openInterest", "n/a") << "\n";
    }
}

void printFirstSpotAssetCtx(const nlohmann::json& spot_meta_and_ctxs) {
    if (!spot_meta_and_ctxs.is_array() || spot_meta_and_ctxs.size() < 2 ||
        !spot_meta_and_ctxs[0].contains("universe") || !spot_meta_and_ctxs[1].is_array()) {
        std::cout << "  Response: " << spot_meta_and_ctxs.dump(2) << "\n";
        return;
    }

    const auto& universe = spot_meta_and_ctxs[0]["universe"];
    const auto& contexts = spot_meta_and_ctxs[1];
    std::cout << "  Spot pairs: " << universe.size() << "\n";

    if (!universe.empty() && !contexts.empty()) {
        const auto& pair = universe[0];
        const auto& ctx = contexts[0];
        std::cout << "  First pair: " << pair.value("name", "<unknown>")
                  << " | markPx: " << ctx.value("markPx", "n/a")
                  << " | midPx: " << ctx.value("midPx", "n/a")
                  << " | dayNtlVlm: " << ctx.value("dayNtlVlm", "n/a") << "\n";
    }
}

void printPerpDexs(const nlohmann::json& perp_dexs) {
    if (!perp_dexs.is_array()) {
        std::cout << "  Response: " << perp_dexs.dump(2) << "\n";
        return;
    }

    std::cout << "  Perp dex entries: " << perp_dexs.size() << "\n";
    const size_t show = std::min(perp_dexs.size(), static_cast<size_t>(5));
    for (size_t i = 0; i < show; ++i) {
        const auto& dex = perp_dexs[i];
        if (dex.is_null()) {
            std::cout << "  - Default dex\n";
            continue;
        }

        std::cout << "  - " << dex.value("name", "<unknown>");
        if (dex.contains("fullName") && !dex["fullName"].is_null()) {
            std::cout << " (" << dex["fullName"].get<std::string>() << ")";
        }
        if (dex.contains("deployer")) {
            std::cout << " | deployer: " << dex["deployer"].get<std::string>();
        }
        std::cout << "\n";
    }
}

void printPerpDeployAuctionStatus(const nlohmann::json& status) {
    if (!status.is_object()) {
        std::cout << "  Response: " << status.dump(2) << "\n";
        return;
    }

    std::cout << "  Start time seconds: " << status.value("startTimeSeconds", 0) << "\n"
              << "  Duration seconds: " << status.value("durationSeconds", 0) << "\n"
              << "  Start gas: " << status.value("startGas", "n/a") << "\n"
              << "  Current gas: " << status.value("currentGas", "n/a") << "\n";

    if (status.contains("endGas") && !status["endGas"].is_null()) {
        std::cout << "  End gas: " << status["endGas"].get<std::string>() << "\n";
    } else {
        std::cout << "  End gas: n/a\n";
    }
}

} // namespace

int main() {
    hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

    std::cout << "=== Perp Metadata and Asset Contexts ===\n";
    auto meta_and_ctxs = info.metaAndAssetCtxs();
    printFirstPerpAssetCtx(meta_and_ctxs);

    std::cout << "\n=== Spot Metadata and Asset Contexts ===\n";
    auto spot_meta_and_ctxs = info.spotMetaAndAssetCtxs();
    printFirstSpotAssetCtx(spot_meta_and_ctxs);

    std::cout << "\n=== Perp Dexs ===\n";
    auto perp_dexs = info.perpDexs();
    printPerpDexs(perp_dexs);

    std::cout << "\n=== Perp Deploy Auction Status ===\n";
    auto auction_status = info.queryPerpDeployAuctionStatus();
    printPerpDeployAuctionStatus(auction_status);

    std::cout << "\nInfo perp deployment queries completed successfully.\n";
    return 0;
}
