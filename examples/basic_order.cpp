#include <hyperliquid/exchange.hpp>
#include <hyperliquid/info.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <cstdlib>

int main() {
    try {
        // Get private key from environment variable
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY=\"0x...\"\n";
            return 1;
        }

        // Create wallet
        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        std::string address = wallet->address();
        std::cout << "Using address: " << address << "\n\n";

        // Create exchange client (testnet)
        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        // Get user state
        std::cout << "Fetching user state...\n";
        auto user_state = exchange.info_.userState(address);
        std::cout << "User state:\n" << user_state.dump(2) << "\n\n";

        // Place a resting limit order
        std::cout << "Placing limit order: Buy 0.2 ETH @ 1100 USDC\n";

        hyperliquid::OrderType order_type;
        order_type.limit = hyperliquid::LimitOrderType{"Gtc"};  // Good til canceled

        auto result = exchange.order(
            "ETH",      // coin
            true,       // is_buy
            0.2,        // size
            1100.0,     // limit_px
            order_type,
            false       // reduce_only
        );

        std::cout << "Order result:\n" << result.dump(2) << "\n\n";

        // Check if order was placed successfully
        if (result["status"] == "ok") {
            auto status = result["response"]["data"]["statuses"][0];
            if (status.contains("resting")) {
                int64_t oid = status["resting"]["oid"];
                std::cout << "✓ Order placed successfully with OID: " << oid << "\n\n";

                // Query order status
                std::cout << "Querying order status...\n";
                auto order_status = exchange.info_.queryOrderByOid(address, oid);
                std::cout << "Order status:\n" << order_status.dump(2) << "\n\n";

                // Cancel the order
                std::cout << "Canceling order OID " << oid << "...\n";
                auto cancel_result = exchange.cancel("ETH", oid);
                std::cout << "Cancel result:\n" << cancel_result.dump(2) << "\n\n";

                if (cancel_result["status"] == "ok") {
                    std::cout << "✓ Order canceled successfully\n";
                }
            } else if (status.contains("filled")) {
                std::cout << "Order filled immediately:\n";
                auto filled = status["filled"];
                std::cout << "  Filled size: " << filled["totalSz"] << "\n";
                std::cout << "  Average price: " << filled["avgPx"] << "\n";
            }
        } else {
            std::cerr << "✗ Order failed\n";
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
