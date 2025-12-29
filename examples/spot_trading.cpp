/**
 * Spot Trading Example
 *
 * This example demonstrates how to trade spot markets on Hyperliquid using C++.
 *
 * Spot markets work differently from perpetual futures:
 * - Spot asset IDs start at 10000 (vs perps starting at 0)
 * - You trade token pairs like "PURR/USDC" or use @{index} notation like "@8"
 * - You need actual token balances to trade (not just margin)
 *
 * Setup:
 *   export HYPERLIQUID_PRIVATE_KEY='0x...'
 *   ./compile.sh examples/spot_trading.cpp spot_trading
 *   ./spot_trading
 */

#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <iostream>
#include <iomanip>
#include <cstdlib>

using namespace hyperliquid;

void printSpotBalances(Info& info, const std::string& address) {
    std::cout << "\n=== Spot Balances ===\n";

    auto spot_state = info.spotUserState(address);

    if (spot_state.contains("balances") && spot_state["balances"].is_array()) {
        auto balances = spot_state["balances"];

        if (balances.empty()) {
            std::cout << "No token balances available\n";
        } else {
            std::cout << std::setw(15) << "Token"
                     << std::setw(20) << "Total"
                     << std::setw(20) << "Available\n";
            std::cout << std::string(55, '-') << "\n";

            for (const auto& balance : balances) {
                std::string coin = balance["coin"].get<std::string>();
                std::string total = balance["total"].get<std::string>();
                std::string hold = balance.value("hold", "0");

                std::cout << std::setw(15) << coin
                         << std::setw(20) << total
                         << std::setw(20) << hold << "\n";
            }
        }
    } else {
        std::cout << "No spot balances\n";
    }
}

void printSpotOpenOrders(Info& info, const std::string& address) {
    std::cout << "\n=== Open Spot Orders ===\n";

    auto open_orders = info.openOrders(address);

    if (open_orders.is_array() && !open_orders.empty()) {
        std::cout << "Open orders:\n";
        std::cout << open_orders.dump(2) << "\n";
    } else {
        std::cout << "No open spot orders\n";
    }
}

int main() {
    std::cout << "=== Hyperliquid Spot Trading Example ===\n\n";

    // Get private key from environment
    const char* private_key_env = std::getenv("HYPERLIQUID_PRIVATE_KEY");
    if (!private_key_env) {
        std::cerr << "Error: HYPERLIQUID_PRIVATE_KEY environment variable not set\n";
        std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY='0x...'\n";
        return 1;
    }

    std::string private_key(private_key_env);

    try {
        // Create wallet
        auto wallet = Wallet::fromPrivateKey(private_key);
        std::cout << "Wallet Address: " << wallet->address() << "\n";

        // Create exchange instance (using testnet for safety)
        // The SDK automatically fetches metadata during construction
        std::cout << "\nInitializing exchange (auto-fetching metadata)...\n";
        Exchange exchange(wallet, TESTNET_API_URL);
        std::cout << "Exchange ready! Metadata loaded automatically.\n";

        // Check spot balances
        printSpotBalances(exchange.info_, wallet->address());

        // Check existing open orders
        printSpotOpenOrders(exchange.info_, wallet->address());

        std::cout << "\n=== Spot Trading Examples ===\n\n";

        // Example 1: Place a limit order for PURR/USDC
        // This uses the token pair name directly
        std::cout << "1. Placing limit buy order for PURR/USDC...\n";
        std::string spot_token = "PURR/USDC";

        try {
            auto order_result = exchange.order(
                spot_token,           // coin: spot token pair
                true,                 // is_buy: true for buy, false for sell
                24.0,                 // sz: amount of base token (PURR)
                0.5,                  // limit_px: price in quote token (USDC)
                OrderType{LimitOrderType{"Gtc"}},  // order_type: Good-til-cancel
                false,                // reduce_only: false for spot
                std::nullopt          // cloid: optional client order ID
            );

            std::cout << "Order Result:\n" << order_result.dump(2) << "\n\n";

            // Cancel the order if it was successfully placed
            if (order_result["status"] == "ok") {
                auto statuses = order_result["response"]["data"]["statuses"];
                if (statuses.is_array() && !statuses.empty()) {
                    auto status = statuses[0];
                    if (status.contains("resting")) {
                        int64_t oid = status["resting"]["oid"].get<int64_t>();

                        std::cout << "Canceling order (oid: " << oid << ")...\n";
                        auto cancel_result = exchange.cancel(spot_token, oid);
                        std::cout << "Cancel Result:\n" << cancel_result.dump(2) << "\n\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error placing order: " << e.what() << "\n";
            std::cerr << "(This is expected if you don't have PURR/USDC available)\n\n";
        }

        // Example 2: Using @{index} notation
        // Spot tokens can also be referenced by their index with @ prefix
        std::cout << "2. Using @index notation (e.g., @8 for KORILA/USDC on testnet)...\n";
        std::string indexed_token = "@8";

        try {
            auto order_result = exchange.order(
                indexed_token,
                true,
                1.0,
                12.0,
                OrderType{LimitOrderType{"Gtc"}},
                false,
                std::nullopt
            );

            std::cout << "Order Result:\n" << order_result.dump(2) << "\n\n";

            // Cancel using the token name (the SDK handles the conversion)
            if (order_result["status"] == "ok") {
                auto statuses = order_result["response"]["data"]["statuses"];
                if (statuses.is_array() && !statuses.empty()) {
                    auto status = statuses[0];
                    if (status.contains("resting")) {
                        int64_t oid = status["resting"]["oid"].get<int64_t>();

                        std::cout << "Canceling order...\n";
                        auto cancel_result = exchange.cancel(indexed_token, oid);
                        std::cout << "Cancel Result:\n" << cancel_result.dump(2) << "\n\n";
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error placing order: " << e.what() << "\n\n";
        }

        // Example 3: Market order (if you have balance)
        std::cout << "3. Market order example (commented out for safety):\n";
        std::cout << R"(
        // Market buy 1 PURR at current market price with 5% slippage
        auto market_result = exchange.marketOpen(
            "PURR/USDC",
            true,      // is_buy
            1.0,       // sz
            std::nullopt,  // px (auto-calculated)
            0.05       // 5% slippage
        );
        )" << "\n\n";

        // Example 4: Spot transfer
        std::cout << "4. Spot token transfer example (commented out for safety):\n";
        std::cout << R"(
        // Transfer 10 PURR to another address
        auto transfer_result = exchange.spotTransfer(
            10.0,                                      // amount
            "0x1234567890123456789012345678901234567890",  // destination
            "PURR"                                     // token name
        );
        )" << "\n\n";

        // Example 5: Fetch and display spot metadata
        std::cout << "5. Fetching and displaying spot metadata...\n";

        auto spot_meta = exchange.info_.spotMeta();
        std::cout << "Available spot tokens: " << spot_meta.tokens.size() << "\n";
        std::cout << "Available spot pairs: " << spot_meta.universe.size() << "\n";

        // Show first few tokens
        if (!spot_meta.tokens.empty()) {
            std::cout << "\nFirst few tokens:\n";
            for (size_t i = 0; i < std::min<size_t>(5, spot_meta.tokens.size()); ++i) {
                const auto& token = spot_meta.tokens[i];
                std::cout << "  " << token.name
                         << " (decimals: " << token.sz_decimals
                         << ", index: " << token.index << ")\n";
            }
        }

        // Show first few pairs
        if (!spot_meta.universe.empty()) {
            std::cout << "\nFirst few spot pairs:\n";
            for (size_t i = 0; i < std::min<size_t>(5, spot_meta.universe.size()); ++i) {
                const auto& pair = spot_meta.universe[i];
                std::cout << "  " << pair.name
                         << " (index: " << pair.index << ")\n";
            }
        }

        std::cout << "\n=== Example Complete ===\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
