#include <hyperliquid/exchange.hpp>
#include <hyperliquid/utils/constants.hpp>
#include <cstdlib>
#include <iostream>

// Rate limits & margin:
//  1. Query the address's current rate limit with userRateLimit().
//  2. Reserve extra request weight with reserveRequestWeight().
//  3. Top up an isolated position to a target leverage with
//     topUpIsolatedOnlyMargin().

int main() {
    try {
        const char* private_key = std::getenv("HYPERLIQUID_PRIVATE_KEY");
        if (!private_key) {
            std::cerr << "Error: Set HYPERLIQUID_PRIVATE_KEY environment variable\n";
            std::cerr << "Usage: export HYPERLIQUID_PRIVATE_KEY=\"0x...\"\n";
            return 1;
        }

        auto wallet = hyperliquid::Wallet::fromPrivateKey(private_key);
        std::string address = wallet->address();
        std::cout << "Using address: " << address << "\n\n";

        hyperliquid::Exchange exchange(wallet, hyperliquid::TESTNET_API_URL);

        // 1. Where does this address stand against the rate limit?
        std::cout << "Current rate limit state:\n";
        auto rate_limit = exchange.info_.userRateLimit(address);
        std::cout << rate_limit.dump(2) << "\n\n";

        // 2. Buy extra request weight instead of getting throttled. The
        //    exchange charges for this; a successful reservation raises the cap.
        const int64_t weight = 10;
        std::cout << "Reserving " << weight << " units of request weight...\n";
        auto reserve_result = exchange.reserveRequestWeight(weight);
        std::cout << reserve_result.dump(2) << "\n\n";

        // Failures come back as HTTP 200 with the error nested in the response,
        // same as other exchange actions — check before assuming success.
        if (reserve_result.value("status", "") == "ok") {
            std::cout << "✓ Request weight reserved\n\n";
        } else {
            std::cerr << "✗ Reservation failed: " << reserve_result.dump() << "\n\n";
        }

        // 3. Top up an isolated position so its effective leverage drops to
        //    the target. Unlike updateIsolatedMargin(amount, coin), which moves
        //    a fixed USDC amount, this lets the exchange compute the margin to
        //    add. Requires an existing isolated position on the coin.
        const std::string coin = "ETH";
        const double target_leverage = 5.0;
        std::cout << "Topping up isolated " << coin << " position to "
                  << target_leverage << "x leverage...\n";
        auto top_up_result = exchange.topUpIsolatedOnlyMargin(coin, target_leverage);
        std::cout << top_up_result.dump(2) << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
