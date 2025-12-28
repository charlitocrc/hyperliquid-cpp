#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>

int main() {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            return 1;
        }

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        std::string coin = "ETH";
        bool is_buy = false;  // Sell
        double sz = 0.05;

        std::cout << "Market " << (is_buy ? "Buy" : "Sell")
                  << " " << sz << " " << coin << "\n\n";

        // Open market position with 1% slippage
        std::cout << "Opening position...\n";
        auto result = exchange.marketOpen(coin, is_buy, sz, std::nullopt, 0.01);

        std::cout << "Result:\n" << result.dump(2) << "\n\n";

        if (result["status"] == "ok") {
            for (const auto& status : result["response"]["data"]["statuses"]) {
                if (status.contains("filled")) {
                    auto filled = status["filled"];
                    std::cout << "✓ Order #" << filled["oid"]
                             << " filled " << filled["totalSz"]
                             << " @ " << filled["avgPx"] << "\n";
                }
            }

            // Wait 2 seconds
            std::cout << "\nWaiting 2 seconds before closing position...\n";
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // Close position
            std::cout << "\nClosing position...\n";
            auto close_result = exchange.marketClose(coin);

            std::cout << "Result:\n" << close_result.dump(2) << "\n\n";

            if (close_result["status"] == "ok") {
                for (const auto& status : close_result["response"]["data"]["statuses"]) {
                    if (status.contains("filled")) {
                        auto filled = status["filled"];
                        std::cout << "✓ Order #" << filled["oid"]
                                 << " filled " << filled["totalSz"]
                                 << " @ " << filled["avgPx"] << "\n";
                    }
                }
            }
        } else {
            std::cerr << "✗ Market order failed\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
