#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        std::string address;

        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <address>\n";
            std::cerr << "Example: " << argv[0] << " 0x1234567890abcdef...\n";
            return 1;
        }

        address = argv[1];

        std::cout << "Querying positions for address: " << address << "\n\n";

        // Create info client (mainnet by default)
        hyperliquid::Info info(hyperliquid::MAINNET_API_URL, true);

        // Get user state
        std::cout << "Fetching user state...\n";
        auto user_state = info.userState(address);

        // Display positions
        std::cout << "\n=== Positions ===\n\n";
        bool has_positions = false;

        for (const auto& asset_pos : user_state["assetPositions"]) {
            auto pos = asset_pos["position"];
            double szi = std::stod(pos["szi"].get<std::string>());

            if (std::abs(szi) > 1e-8) {  // Has position
                has_positions = true;
                std::cout << "Coin: " << pos["coin"].get<std::string>() << "\n"
                         << "  Size: " << pos["szi"].get<std::string>()
                         << " (" << (szi > 0 ? "LONG" : "SHORT") << ")\n"
                         << "  Entry Price: " << pos["entryPx"].get<std::string>() << "\n"
                         << "  Unrealized PnL: " << pos["unrealizedPnl"].get<std::string>() << "\n"
                         << "  Margin Used: " << pos["marginUsed"].get<std::string>() << "\n"
                         << "\n";
            }
        }

        if (!has_positions) {
            std::cout << "No open positions\n\n";
        }

        // Display margin summary
        if (user_state.contains("marginSummary")) {
            auto margin = user_state["marginSummary"];
            std::cout << "=== Margin Summary ===\n\n"
                     << "Account Value: " << margin["accountValue"].get<std::string>() << " USDC\n"
                     << "Total Margin Used: " << margin["totalMarginUsed"].get<std::string>() << " USDC\n"
                     << "Total Position Value: " << margin["totalNtlPos"].get<std::string>() << " USDC\n\n";
        }

        // Get open orders
        std::cout << "=== Open Orders ===\n\n";
        auto open_orders = info.openOrders(address);

        if (open_orders.is_array() && !open_orders.empty()) {
            std::cout << "Found " << open_orders.size() << " open orders:\n\n";
            for (const auto& order : open_orders) {
                std::cout << "OID " << order["oid"] << ": "
                         << order["coin"].get<std::string>() << " "
                         << order["side"].get<std::string>() << " "
                         << order["sz"].get<std::string>() << " @ "
                         << order["limitPx"].get<std::string>() << "\n";
            }
        } else {
            std::cout << "No open orders\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
