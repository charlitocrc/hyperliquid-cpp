#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <cstdlib>
#include <vector>

int main() {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            return 1;
        }

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);

        // Exchange automatically fetches metadata during construction
        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        std::cout << "Creating buy ladder for ETH...\n\n";

        // Create multiple orders
        std::vector<hyperliquid::OrderRequest> orders;

        hyperliquid::OrderType limit_gtc;
        limit_gtc.limit = hyperliquid::LimitOrderType{"Gtc"};

        // Buy ladder at different price levels
        orders.push_back({
            "ETH",      // coin
            true,       // is_buy
            0.1,        // sz
            1100.0,     // limit_px
            limit_gtc,
            false       // reduce_only
        });

        orders.push_back({
            "ETH",
            true,
            0.1,
            1090.0,
            limit_gtc,
            false
        });

        orders.push_back({
            "ETH",
            true,
            0.1,
            1080.0,
            limit_gtc,
            false
        });

        std::cout << "Placing " << orders.size() << " orders in one request...\n";
        auto result = exchange.bulkOrders(orders);

        std::cout << "\nResult:\n" << result.dump(2) << "\n\n";

        // Extract OIDs for later management
        if (result["status"] == "ok") {
            std::vector<int64_t> oids;
            for (const auto& status : result["response"]["data"]["statuses"]) {
                if (status.contains("resting")) {
                    oids.push_back(status["resting"]["oid"]);
                }
            }

            if (!oids.empty()) {
                std::cout << "✓ Successfully placed " << oids.size() << " orders\n";
                std::cout << "Order IDs: ";
                for (size_t i = 0; i < oids.size(); ++i) {
                    std::cout << oids[i];
                    if (i < oids.size() - 1) std::cout << ", ";
                }
                std::cout << "\n\n";

                // Example: Cancel all placed orders
                std::cout << "Canceling all placed orders...\n";
                std::vector<hyperliquid::CancelRequest> cancels;
                for (int64_t oid : oids) {
                    cancels.push_back({"ETH", oid});
                }

                auto cancel_result = exchange.bulkCancel(cancels);
                std::cout << "Cancel result:\n" << cancel_result.dump(2) << "\n\n";

                if (cancel_result["status"] == "ok") {
                    std::cout << "✓ All orders canceled successfully\n";
                }
            }
        } else {
            std::cerr << "✗ Bulk order placement failed\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
