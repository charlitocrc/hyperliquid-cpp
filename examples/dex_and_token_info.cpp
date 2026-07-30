#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>

// Read-only queries about a builder-deployed (HIP-3) perp dex and a spot token.
// No private key needed.
//
// Covers perpDexLimits(), perpDexStatus(), tokenDetails(), and the margin-table
// fields that meta() now parses out of the perp metadata.

int main() {
    try {
        hyperliquid::Info info(hyperliquid::MAINNET_API_URL);

        // A builder dex. info.perpDexs() lists the deployed ones; entry [0] is
        // null for the default dex, which has no builder limits.
        const std::string dex = "xyz";

        std::cout << "=== perpDexLimits(\"" << dex << "\") ===\n";
        auto limits = info.perpDexLimits(dex);
        std::cout << "  totalOiCap:     " << limits.value("totalOiCap", "?") << "\n";
        std::cout << "  oiSzCapPerPerp: " << limits.value("oiSzCapPerPerp", "?") << "\n";
        std::cout << "  maxTransferNtl: " << limits.value("maxTransferNtl", "?") << "\n";
        std::cout << "  per-coin OI caps: " << limits["coinToOiCap"].size() << " entries\n";
        for (size_t i = 0; i < 3 && i < limits["coinToOiCap"].size(); ++i) {
            const auto& entry = limits["coinToOiCap"][i];
            std::cout << "    " << entry[0].get<std::string>()
                      << " -> " << entry[1].get<std::string>() << "\n";
        }

        // Unlike most dex parameters, "" is rejected here rather than meaning
        // the default dex.
        std::cout << "\n=== perpDexStatus(\"" << dex << "\") ===\n";
        auto status = info.perpDexStatus(dex);
        std::cout << "  totalNetDeposit: " << status.value("totalNetDeposit", "?") << "\n";

        // Margin tables live on the meta response. Assets point at one by id;
        // a table is a ladder of (lowerBound notional -> max leverage).
        std::cout << "\n=== meta(\"" << dex << "\") margin tables ===\n";
        auto meta = info.meta(dex);
        std::cout << "  assets: " << meta.universe.size()
                  << ", margin tables: " << meta.margin_tables.size()
                  << ", collateral token index: " << meta.collateral_token << "\n";

        for (size_t i = 0; i < 3 && i < meta.universe.size(); ++i) {
            const auto& asset = meta.universe[i];
            std::cout << "  " << asset.name
                      << "  maxLeverage=" << asset.max_leverage
                      << "  marginTableId="
                      << (asset.margin_table_id ? std::to_string(*asset.margin_table_id) : "-")
                      << "  marginMode=" << asset.margin_mode.value_or("-")
                      << "  growthMode=" << asset.growth_mode.value_or("-")
                      << (asset.is_delisted ? "  [delisted]" : "") << "\n";
        }

        // Tiered tables -- ones where leverage steps down as position notional
        // grows -- mostly live on the default dex, so query that separately.
        // A builder dex often has a single flat table.
        std::cout << "\n=== meta(\"\") tiered margin tables ===\n";
        auto default_meta = info.meta("");
        for (const auto& table : default_meta.margin_tables) {
            if (table.margin_tiers.size() < 2) {
                continue;
            }
            std::cout << "  table " << table.id << " (" << table.description << "):\n";
            for (const auto& tier : table.margin_tiers) {
                std::cout << "    >= " << tier.lower_bound
                          << " notional -> " << tier.max_leverage << "x\n";
            }
        }

        // Funding across venues, for the first perp dex only. A raw rate is
        // not comparable between venues without scaling by the interval.
        std::cout << "\n=== predictedFundings (first 2 coins) ===\n";
        auto fundings = info.predictedFundings();
        for (size_t i = 0; i < 2 && i < fundings.size(); ++i) {
            std::cout << "  " << fundings[i][0].get<std::string>() << ":\n";
            for (const auto& venue : fundings[i][1]) {
                std::cout << "    " << venue[0].get<std::string>()
                          << "  rate=" << venue[1].value("fundingRate", "?")
                          << "  every " << venue[1].value("fundingIntervalHours", 0) << "h\n";
            }
        }

        std::cout << "\n=== perpsAtOpenInterestCap ===\n";
        auto capped = info.perpsAtOpenInterestCap();
        std::cout << "  default dex: " << capped.size() << " capped -> " << capped.dump() << "\n";
        std::cout << "  " << dex << ": " << info.perpsAtOpenInterestCap(dex).dump() << "\n";

        std::cout << "\n=== perpCategories / perpConciseAnnotations ===\n";
        auto categories = info.perpCategories();
        std::cout << "  annotated coins: " << categories.size() << "\n";
        for (size_t i = 0; i < 3 && i < categories.size(); ++i) {
            std::cout << "    " << categories[i][0].get<std::string>()
                      << " -> " << categories[i][1].get<std::string>() << "\n";
        }
        auto concise = info.perpConciseAnnotations();
        std::cout << "  concise entries: " << concise.size()
                  << " (same coins, plus displayName/keywords where set)\n";

        // Full annotation for one coin. Plain default-dex coins such as "BTC"
        // have none and come back null.
        std::cout << "\n=== perpAnnotation ===\n";
        for (const std::string coin : {"xyz:TSLA", "BTC"}) {
            auto annotation = info.perpAnnotation(coin);
            std::cout << "  " << coin << ": ";
            if (annotation.is_null()) {
                std::cout << "(no annotation)\n";
            } else {
                std::cout << annotation.value("category", "?") << " -- "
                          << annotation.value("description", "").substr(0, 60) << "...\n";
            }
        }

        // PURR's onchain token id, as listed in info.spotMeta().tokens.
        std::cout << "\n=== tokenDetails(PURR) ===\n";
        auto token = info.tokenDetails("0xc1fb593aeffbeb02f85e0308e9956a90");
        std::cout << "  name:              " << token.value("name", "?") << "\n";
        std::cout << "  totalSupply:       " << token.value("totalSupply", "?") << "\n";
        std::cout << "  circulatingSupply: " << token.value("circulatingSupply", "?") << "\n";
        std::cout << "  markPx:            " << token.value("markPx", "?") << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
